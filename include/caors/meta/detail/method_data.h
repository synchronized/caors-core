// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_METHOD_DATA_H_
#define CAORS_META_DETAIL_METHOD_DATA_H_

#include "caors/core/containers.h"
#include "caors/memory/resource.h"
#include "caors/memory/unique_ptr.h"
#include "caors/meta/detail/argument_access.h"
#include "caors/meta/detail/value_conversion.h"
#include "caors/meta/metadata.h"
#include "caors/meta/method.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

class method_data;

struct method_resolution {
  const method_data* method = nullptr;
  method_match_kind kind = method_match_kind::none;

  [[nodiscard]] bool matched() const noexcept {
    return kind == method_match_kind::exact || kind == method_match_kind::conversion;
  }
};

[[nodiscard]] constexpr method_match_kind
method_match_kind_from_rank(argument_match_rank rank) noexcept {
  switch (rank) {
  case argument_match_rank::exact:
    return method_match_kind::exact;
  case argument_match_rank::conversion:
    return method_match_kind::conversion;
  case argument_match_rank::none:
  default:
    return method_match_kind::none;
  }
}

/** @brief 方法访问函数表，负责返回类型信息并执行底层成员函数调用。 */
struct method_ops {
  using type_fn = meta::type (*)(const method_data& method) noexcept;
  using parameter_count_fn = std::size_t (*)(const method_data& method) noexcept;
  using parameter_type_fn = std::optional<meta::type> (*)(const method_data& method,
                                                          std::size_t index) noexcept;
  using rank_arguments_fn = argument_match_rank (*)(const method_data& method,
                                                    std::span<const argument> arguments,
                                                    argument_match_options options) noexcept;
  using prepare_arguments_fn = vector<argument> (*)(const method_data& method,
                                                    std::span<const argument> arguments,
                                                    argument_match_options options);
  using invoke_fn = invoke_result (*)(const method_data& method, const void* object,
                                      bool mutable_object, std::span<const argument> arguments);
  using rank_value_arguments_fn = argument_match_rank (*)(const method_data& method,
                                                          std::span<const meta::value> arguments,
                                                          argument_match_options options) noexcept;
  using invoke_value_fn = invoke_value_result (*)(const method_data& method, const void* object,
                                                  bool mutable_object,
                                                  std::span<const meta::value> arguments);

  type_fn get_owner_type = nullptr;
  type_fn get_return_type = nullptr;
  parameter_count_fn parameter_count = nullptr;
  parameter_type_fn get_parameter_type = nullptr;
  rank_arguments_fn rank_arguments = nullptr;
  prepare_arguments_fn prepare_arguments = nullptr;
  invoke_fn invoke = nullptr;
  rank_value_arguments_fn rank_value_arguments = nullptr;
  invoke_value_fn invoke_value = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return get_owner_type != nullptr && get_return_type != nullptr && parameter_count != nullptr &&
           get_parameter_type != nullptr && rank_arguments != nullptr &&
           prepare_arguments != nullptr && invoke != nullptr && rank_value_arguments != nullptr &&
           invoke_value != nullptr;
  }
};

/**
 * @brief 底层成员方法反射元信息。
 *
 * 方法调用支持成员函数指针、全局函数指针和 module 级 callable。参数通过 `argument` 传入，
 * 返回值通过 `invoke_result` 区分失败、`void` 成功、拥有值返回和引用返回。
 */
class method_data final {
private:
  struct callable_storage_base {
    virtual ~callable_storage_base() = default;
  };

  template <typename Callable> struct callable_storage_model final : callable_storage_base {
    template <typename Value>
    explicit callable_storage_model(Value&& stored) : value(std::forward<Value>(stored)) {}

    Callable value;
  };

public:
  static constexpr std::size_t storage_size = sizeof(void*) * 4U;

  method_data() noexcept = default;

  method_data(string_name method_name, method_ops method_operations, bool is_const_method) noexcept
      : name_(method_name), ops_(method_operations), const_method_(is_const_method) {}

  method_data(const method_data& other)
      : name_(other.name_), storage_(other.storage_), ops_(other.ops_),
        const_method_(other.const_method_), callable_(other.callable_),
        default_arguments_(other.default_arguments_), parameter_names_(other.parameter_names_) {
    if (other.metadata_ != nullptr) {
      metadata_ =
          memory::make_unique<meta::metadata_map>(&memory::library_resource(), *other.metadata_);
    }
  }

  method_data& operator=(const method_data& other) {
    if (this == &other) {
      return *this;
    }

    name_ = other.name_;
    storage_ = other.storage_;
    ops_ = other.ops_;
    const_method_ = other.const_method_;
    callable_ = other.callable_;
    default_arguments_ = other.default_arguments_;
    parameter_names_ = other.parameter_names_;
    if (other.metadata_ != nullptr) {
      metadata_ =
          memory::make_unique<meta::metadata_map>(&memory::library_resource(), *other.metadata_);
    } else {
      metadata_.reset();
    }
    return *this;
  }

  method_data(method_data&&) noexcept = default;
  method_data& operator=(method_data&&) noexcept = default;
  ~method_data() = default;

  [[nodiscard]] bool valid() const noexcept { return !name_.empty() && ops_.valid(); }

  [[nodiscard]] string_name name() const noexcept { return name_; }
  [[nodiscard]] bool const_method() const noexcept { return const_method_; }
  [[nodiscard]] const meta::metadata_map& metadata() const noexcept {
    return metadata_ != nullptr ? *metadata_ : empty_metadata_map();
  }

  [[nodiscard]] const meta::metadata_value* find_metadata(meta::metadata_key key) const noexcept {
    return metadata_ != nullptr ? meta::find_metadata(*metadata_, key) : nullptr;
  }

  void set_metadata(meta::metadata_key key, meta::metadata_value value) {
    if (metadata_ == nullptr) {
      metadata_ = memory::make_unique<meta::metadata_map>(
          &memory::library_resource(), meta::make_metadata_map(&memory::library_resource()));
    }
    meta::set_metadata(*metadata_, key, std::move(value));
  }

  [[nodiscard]] meta::type get_owner_type() const noexcept { return ops_.get_owner_type(*this); }
  [[nodiscard]] meta::type get_return_type() const noexcept { return ops_.get_return_type(*this); }
  [[nodiscard]] std::size_t parameter_count() const noexcept { return ops_.parameter_count(*this); }

  [[nodiscard]] std::optional<meta::type> get_parameter_type(std::size_t index) const noexcept {
    return ops_.get_parameter_type(*this, index);
  }

  [[nodiscard]] string_name get_parameter_name(std::size_t index) const noexcept {
    return index < parameter_names_.size() ? parameter_names_[index] : string_name{};
  }

  [[nodiscard]] bool matches_arguments(std::span<const argument> arguments,
                                       argument_match_options options = {}) const noexcept {
    return match_rank(arguments, options) != argument_match_rank::none;
  }

  [[nodiscard]] bool matches_arguments(std::span<const meta::value> arguments,
                                       argument_match_options options = {}) const noexcept {
    return match_rank(arguments, options) != argument_match_rank::none;
  }

  [[nodiscard]] bool matches_arguments(std::span<const named_argument> arguments,
                                       argument_match_options options = {}) const noexcept {
    return match_rank(arguments, options) != argument_match_rank::none;
  }

  [[nodiscard]] argument_match_rank match_rank(std::span<const argument> arguments,
                                               argument_match_options options = {}) const noexcept {
    if (ops_.rank_arguments(*this, arguments, {}) == argument_match_rank::exact) {
      return argument_match_rank::exact;
    }

    const argument_match_rank direct_rank = ops_.rank_arguments(*this, arguments, options);
    if (direct_rank != argument_match_rank::none) {
      return direct_rank;
    }

    vector<argument> completed = complete_positional_arguments(arguments, options);
    return !completed.empty() ? ops_.rank_arguments(*this, completed, options)
                              : argument_match_rank::none;
  }

  [[nodiscard]] argument_match_rank match_rank(std::span<const meta::value> arguments,
                                               argument_match_options options = {}) const noexcept {
    if (ops_.rank_value_arguments(*this, arguments, {}) == argument_match_rank::exact) {
      return argument_match_rank::exact;
    }

    const argument_match_rank direct_rank = ops_.rank_value_arguments(*this, arguments, options);
    if (direct_rank != argument_match_rank::none) {
      return direct_rank;
    }

    vector<meta::value> completed = complete_value_arguments(arguments, options);
    return !completed.empty() ? ops_.rank_value_arguments(*this, completed, options)
                              : argument_match_rank::none;
  }

  [[nodiscard]] argument_match_rank match_rank(std::span<const named_argument> arguments,
                                               argument_match_options options = {}) const noexcept {
    vector<argument> completed = normalize_named_arguments(arguments, options);
    return !completed.empty() ? ops_.rank_arguments(*this, completed, options)
                              : argument_match_rank::none;
  }

  [[nodiscard]] bool same_signature(const method_data& other) const noexcept {
    if (!valid() || !other.valid() || name() != other.name() ||
        const_method() != other.const_method() || parameter_count() != other.parameter_count()) {
      return false;
    }

    for (std::size_t index = 0; index < parameter_count(); ++index) {
      const std::optional<meta::type> left = get_parameter_type(index);
      const std::optional<meta::type> right = other.get_parameter_type(index);
      if (!left.has_value() || !right.has_value() || left->id() != right->id()) {
        return false;
      }
    }

    return true;
  }

  [[nodiscard]] invoke_result invoke_result_of(const void* object, bool mutable_object,
                                               std::span<const argument> arguments,
                                               argument_match_options options = {}) const {
    if (get_owner_type()) {
      if (object == nullptr) {
        return invoke_result::failed(invoke_failure_reason::invalid_object);
      }
      if (!const_method_ && !mutable_object) {
        return invoke_result::failed(invoke_failure_reason::const_violation);
      }
    }

    vector<argument> completed = prepare_positional_arguments(arguments, options);
    if (completed.empty() && parameter_count() > 0) {
      return invoke_result::failed(invoke_failure_reason::argument_mismatch);
    }

    invoke_result result = ops_.invoke(*this, object, mutable_object, completed);
    return result ? std::move(result)
                  : invoke_result::failed(invoke_failure_reason::invocation_failed);
  }

  [[nodiscard]] invoke_value_result
  invoke_value_result_of(const void* object, bool mutable_object,
                         std::span<const meta::value> arguments,
                         argument_match_options options = {}) const {
    if (get_owner_type()) {
      if (object == nullptr) {
        return invoke_value_result::failed(invoke_failure_reason::invalid_object);
      }
      if (!const_method_ && !mutable_object) {
        return invoke_value_result::failed(invoke_failure_reason::const_violation);
      }
    }

    vector<meta::value> completed = prepare_value_arguments(arguments, options);
    if (completed.empty() && parameter_count() > 0) {
      return invoke_value_result::failed(invoke_failure_reason::argument_mismatch);
    }

    invoke_value_result result = ops_.invoke_value(*this, object, mutable_object, completed);
    return result ? std::move(result)
                  : invoke_value_result::failed(invoke_failure_reason::invocation_failed);
  }

  [[nodiscard]] invoke_value_result
  invoke_runtime_value(const void* object, bool mutable_object,
                       std::span<const meta::value> arguments,
                       argument_match_options options = {}) const;

  [[nodiscard]] invoke_result invoke_result_of(const void* object, bool mutable_object,
                                               std::span<const named_argument> arguments,
                                               argument_match_options options = {}) const {
    if (get_owner_type()) {
      if (object == nullptr) {
        return invoke_result::failed(invoke_failure_reason::invalid_object);
      }
      if (!const_method_ && !mutable_object) {
        return invoke_result::failed(invoke_failure_reason::const_violation);
      }
    }

    vector<argument> completed = normalize_named_arguments(arguments, options);
    if (completed.empty()) {
      return invoke_result::failed(invoke_failure_reason::argument_mismatch);
    }

    vector<argument> prepared = ops_.prepare_arguments(*this, completed, options);
    if (prepared.empty() && parameter_count() > 0) {
      return invoke_result::failed(invoke_failure_reason::argument_mismatch);
    }

    invoke_result result = ops_.invoke(*this, object, mutable_object, prepared);
    return result ? std::move(result)
                  : invoke_result::failed(invoke_failure_reason::invocation_failed);
  }

  [[nodiscard]] any invoke(const void* object, bool mutable_object,
                           std::span<const argument> arguments) const {
    invoke_result result = invoke_result_of(object, mutable_object, arguments);
    return result.has_value() ? std::move(result.mutable_value()) : any{};
  }

  template <typename Member> [[nodiscard]] Member load_member_pointer() const noexcept {
    Member member{};
    std::memcpy(&member, storage_.data(), sizeof(Member));
    return member;
  }

  template <typename Member> void store_member_pointer(Member member) noexcept {
    static_assert(std::is_trivially_copyable_v<Member>,
                  "Member pointers must be trivially copyable");
    static_assert(sizeof(Member) <= storage_size,
                  "Member pointer is too large for meta method storage");
    std::memcpy(storage_.data(), &member, sizeof(Member));
  }

  template <typename Callable> void store_callable(Callable&& callable) {
    using value_type = std::decay_t<Callable>;
    callable_ =
        std::make_shared<callable_storage_model<value_type>>(std::forward<Callable>(callable));
  }

  template <typename Callable> [[nodiscard]] const Callable* callable() const noexcept {
    const auto* storage = dynamic_cast<const callable_storage_model<Callable>*>(callable_.get());
    return storage != nullptr ? &storage->value : nullptr;
  }

  [[nodiscard]] std::size_t default_argument_count() const noexcept {
    return default_arguments_.size();
  }

  [[nodiscard]] bool add_default_argument(any value) {
    if (!value || default_arguments_.size() >= parameter_count()) {
      return false;
    }

    const std::size_t parameter_index = parameter_count() - default_arguments_.size() - 1U;
    const std::optional<meta::type> parameter_type = get_parameter_type(parameter_index);
    if (!parameter_type.has_value() || !*parameter_type ||
        parameter_type->id() != value.get_type().id()) {
      return false;
    }

    default_arguments_.push_back(std::move(value));
    return true;
  }

  [[nodiscard]] bool add_parameter_name(string_name name) {
    if (!name || parameter_names_.size() >= parameter_count()) {
      return false;
    }

    for (string_name existing : parameter_names_) {
      if (existing == name) {
        return false;
      }
    }

    parameter_names_.push_back(name);
    return true;
  }

private:
  [[nodiscard]] static const meta::metadata_map& empty_metadata_map() noexcept {
    static const meta::metadata_map empty{
        typename meta::metadata_map::allocator_type{&memory::library_resource()}};
    return empty;
  }

  [[nodiscard]] const any* default_argument_for_parameter(std::size_t parameter_index) const {
    const std::size_t count = parameter_count();
    if (parameter_index >= count || count - parameter_index > default_arguments_.size()) {
      return nullptr;
    }

    const std::size_t default_index = count - parameter_index - 1U;
    return &default_arguments_[default_index];
  }

  [[nodiscard]] vector<argument>
  complete_positional_arguments(std::span<const argument> arguments,
                                argument_match_options options) const {
    if (!options.allow_default_arguments || arguments.size() >= parameter_count()) {
      return {};
    }

    const std::size_t missing_count = parameter_count() - arguments.size();
    if (missing_count > default_arguments_.size()) {
      return {};
    }

    vector<argument> completed{};
    completed.reserve(parameter_count());
    for (const argument& value : arguments) {
      completed.push_back(value);
    }

    const std::size_t first_default = missing_count - 1U;
    for (std::size_t index = 0; index < missing_count; ++index) {
      const any& value = default_arguments_[first_default - index];
      completed.push_back(argument::from(value.ref()));
    }

    return completed;
  }

  [[nodiscard]] vector<meta::value> complete_value_arguments(std::span<const meta::value> arguments,
                                                             argument_match_options options) const {
    if (!options.allow_default_arguments || arguments.size() >= parameter_count()) {
      return {};
    }

    const std::size_t missing_count = parameter_count() - arguments.size();
    if (missing_count > default_arguments_.size()) {
      return {};
    }

    vector<meta::value> completed{};
    completed.reserve(parameter_count());
    for (const meta::value& value : arguments) {
      completed.push_back(value);
    }

    const std::size_t first_default = missing_count - 1U;
    for (std::size_t index = 0; index < missing_count; ++index) {
      const any& value = default_arguments_[first_default - index];
      std::optional<meta::value> fast_value = try_make_value_from_any(value);
      if (!fast_value.has_value()) {
        return {};
      }
      completed.push_back(std::move(*fast_value));
    }

    return completed;
  }

  [[nodiscard]] vector<meta::value> prepare_value_arguments(std::span<const meta::value> arguments,
                                                            argument_match_options options) const {
    if (ops_.rank_value_arguments(*this, arguments, {}) == argument_match_rank::exact ||
        ops_.rank_value_arguments(*this, arguments, options) != argument_match_rank::none) {
      return vector<meta::value>{arguments.begin(), arguments.end()};
    }

    return complete_value_arguments(arguments, options);
  }

  [[nodiscard]] vector<argument>
  prepare_positional_arguments(std::span<const argument> arguments,
                               argument_match_options options) const {
    if (ops_.rank_arguments(*this, arguments, {}) == argument_match_rank::exact) {
      return ops_.prepare_arguments(*this, arguments, {});
    }

    vector<argument> prepared = ops_.prepare_arguments(*this, arguments, options);
    if (!prepared.empty() || parameter_count() == 0) {
      return prepared;
    }

    vector<argument> completed = complete_positional_arguments(arguments, options);
    if (completed.empty()) {
      return {};
    }

    return ops_.prepare_arguments(*this, completed, options);
  }

  [[nodiscard]] vector<argument>
  normalize_named_arguments(std::span<const named_argument> arguments,
                            argument_match_options options) const {
    if (!options.allow_named_arguments || parameter_count() == 0 ||
        arguments.size() > parameter_count()) {
      return {};
    }

    vector<argument> completed{};
    completed.reserve(parameter_count());
    vector<bool> assigned(parameter_count(), false);
    for (std::size_t index = 0; index < parameter_count(); ++index) {
      completed.push_back({});
    }

    for (const named_argument& input : arguments) {
      if (!input) {
        return {};
      }

      bool found = false;
      for (std::size_t index = 0; index < parameter_count(); ++index) {
        if (get_parameter_name(index) != input.name) {
          continue;
        }

        if (assigned[index]) {
          return {};
        }

        completed[index] = input.value;
        assigned[index] = true;
        found = true;
        break;
      }

      if (!found) {
        return {};
      }
    }

    for (std::size_t index = 0; index < parameter_count(); ++index) {
      if (assigned[index]) {
        continue;
      }

      const any* default_value = default_argument_for_parameter(index);
      if (default_value == nullptr || !options.allow_default_arguments) {
        return {};
      }

      completed[index] = argument::from(default_value->ref());
    }

    return completed;
  }

  string_name name_{};
  alignas(std::max_align_t) std::array<std::byte, storage_size> storage_{};
  method_ops ops_{};
  bool const_method_ = false;
  std::shared_ptr<const callable_storage_base> callable_{};
  vector<any> default_arguments_{};
  vector<string_name> parameter_names_{};
  memory::unique_ptr<meta::metadata_map> metadata_{};
};

/** @brief 同名成员方法的一组重载。 */
class method_set_data final {
public:
  method_set_data() noexcept = default;
  explicit method_set_data(string_name method_name) noexcept : name_(method_name) {}

  [[nodiscard]] bool valid() const noexcept {
    if (!name_ || overloads_.empty()) {
      return false;
    }

    for (const method_data& method : overloads_) {
      if (!method.valid() || method.name() != name_) {
        return false;
      }
    }

    return true;
  }

  [[nodiscard]] string_name name() const noexcept { return name_; }
  [[nodiscard]] std::size_t overload_count() const noexcept { return overloads_.size(); }

  [[nodiscard]] const method_data* overload_at(std::size_t index) const noexcept {
    return index < overloads_.size() ? &overloads_[index] : nullptr;
  }

  [[nodiscard]] method_data* mutable_overload_at(std::size_t index) noexcept {
    return index < overloads_.size() ? &overloads_[index] : nullptr;
  }

  [[nodiscard]] const method_data* single() const noexcept {
    return overloads_.size() == 1U ? &overloads_.front() : nullptr;
  }

  [[nodiscard]] const method_data* resolve(std::span<const argument> arguments,
                                           argument_match_options options = {}) const noexcept {
    return match(arguments, options).method;
  }

  [[nodiscard]] method_resolution match(std::span<const argument> arguments,
                                        argument_match_options options = {}) const noexcept {
    const method_data* matched = nullptr;
    argument_match_rank best_rank = argument_match_rank::none;
    for (const method_data& method : overloads_) {
      const argument_match_rank rank = method.match_rank(arguments, options);
      if (rank == argument_match_rank::none) {
        continue;
      }

      if (matched != nullptr && rank == best_rank) {
        return method_resolution{nullptr, method_match_kind::ambiguous};
      }

      if (matched == nullptr ||
          static_cast<unsigned char>(rank) > static_cast<unsigned char>(best_rank)) {
        matched = &method;
        best_rank = rank;
      }
    }

    return method_resolution{matched, method_match_kind_from_rank(best_rank)};
  }

  [[nodiscard]] const method_data* resolve(std::span<const meta::value> arguments,
                                           argument_match_options options = {}) const noexcept {
    return match(arguments, options).method;
  }

  [[nodiscard]] method_resolution match(std::span<const meta::value> arguments,
                                        argument_match_options options = {}) const noexcept {
    const method_data* matched = nullptr;
    argument_match_rank best_rank = argument_match_rank::none;
    for (const method_data& method : overloads_) {
      const argument_match_rank rank = method.match_rank(arguments, options);
      if (rank == argument_match_rank::none) {
        continue;
      }

      if (matched != nullptr && rank == best_rank) {
        return method_resolution{nullptr, method_match_kind::ambiguous};
      }

      if (matched == nullptr ||
          static_cast<unsigned char>(rank) > static_cast<unsigned char>(best_rank)) {
        matched = &method;
        best_rank = rank;
      }
    }

    return method_resolution{matched, method_match_kind_from_rank(best_rank)};
  }

  [[nodiscard]] invoke_value_result
  invoke_runtime_value(const void* object, bool mutable_object,
                       std::span<const meta::value> arguments,
                       argument_match_options options = {}) const;

  [[nodiscard]] const method_data* resolve(std::span<const named_argument> arguments,
                                           argument_match_options options = {}) const noexcept {
    return match(arguments, options).method;
  }

  [[nodiscard]] method_resolution match(std::span<const named_argument> arguments,
                                        argument_match_options options = {}) const noexcept {
    const method_data* matched = nullptr;
    argument_match_rank best_rank = argument_match_rank::none;
    for (const method_data& method : overloads_) {
      const argument_match_rank rank = method.match_rank(arguments, options);
      if (rank == argument_match_rank::none) {
        continue;
      }

      if (matched != nullptr && rank == best_rank) {
        return method_resolution{nullptr, method_match_kind::ambiguous};
      }

      if (matched == nullptr ||
          static_cast<unsigned char>(rank) > static_cast<unsigned char>(best_rank)) {
        matched = &method;
        best_rank = rank;
      }
    }

    return method_resolution{matched, method_match_kind_from_rank(best_rank)};
  }

  [[nodiscard]] bool contains_signature(const method_data& method) const noexcept {
    for (const method_data& overload : overloads_) {
      if (overload.same_signature(method)) {
        return true;
      }
    }

    return false;
  }

  [[nodiscard]] std::optional<std::size_t> add_overload(method_data method) {
    if (!method.valid() || method.name() != name_ || contains_signature(method)) {
      return std::nullopt;
    }

    overloads_.push_back(std::move(method));
    return overloads_.size() - 1U;
  }

private:
  string_name name_{};
  vector<method_data> overloads_{};
};

} // namespace caors::meta::detail

#endif
