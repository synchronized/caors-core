// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_METHOD_RECORD_H_
#define CAORS_META_DETAIL_METHOD_RECORD_H_

#include "caors/core/containers.h"
#include "caors/memory/resource.h"
#include "caors/memory/unique_ptr.h"
#include "caors/meta/detail/argument_access.h"
#include "caors/meta/detail/request_payload.h"
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

class method_record;

struct method_resolution {
  const method_record* method = nullptr;
  method_match_kind kind = method_match_kind::none;

  [[nodiscard]] bool matched() const noexcept {
    return kind == method_match_kind::exact || kind == method_match_kind::conversion;
  }
};

struct method_invocation_resolution {
  const method_record* method = nullptr;
  method_match_kind kind = method_match_kind::none;
  invoke_failure_reason failure_reason = invoke_failure_reason::argument_mismatch;

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

enum class method_request_kind : std::uint32_t {
  owner_type = 1,
  return_type = 2,
  parameter_count = 3,
  parameter_type = 4,
  rank_arguments = 5,
  prepare_arguments = 6,
  invoke = 7,
  rank_value_arguments = 8,
  invoke_value = 9,
};

struct method_type_request {
  const method_record* method = nullptr;
  meta::type result{};
};

struct method_parameter_count_request {
  const method_record* method = nullptr;
  std::size_t result = 0;
};

struct method_parameter_type_request {
  const method_record* method = nullptr;
  std::size_t index = 0;
  std::optional<meta::type> result{};
};

struct method_rank_arguments_request {
  const method_record* method = nullptr;
  std::span<const argument> arguments{};
  argument_match_options options{};
  argument_match_rank result = argument_match_rank::none;
};

struct method_prepare_arguments_request {
  const method_record* method = nullptr;
  std::span<const argument> arguments{};
  argument_match_options options{};
  vector<argument> result{};
};

struct method_invoke_request {
  const method_record* method = nullptr;
  const void* object = nullptr;
  bool mutable_object = false;
  std::span<const argument> arguments{};
  invoke_result result{};
};

struct method_rank_value_arguments_request {
  const method_record* method = nullptr;
  std::span<const meta::value> arguments{};
  argument_match_options options{};
  argument_match_rank result = argument_match_rank::none;
};

struct method_invoke_value_request {
  const method_record* method = nullptr;
  const void* object = nullptr;
  bool mutable_object = false;
  std::span<const meta::value> arguments{};
  invoke_value_result result{};
};

struct method_callable_storage_base {
  virtual ~method_callable_storage_base() = default;
};

template <typename Callable>
struct method_callable_storage_model final : method_callable_storage_base {
  template <typename Value>
  explicit method_callable_storage_model(Value&& stored) : value(std::forward<Value>(stored)) {}

  Callable value;
};

struct method_wrapper_state {
  static constexpr std::size_t storage_size = sizeof(void*) * 4U;

  explicit method_wrapper_state(memory::memory_resource* resource) noexcept
      : member_resource(resource != nullptr ? resource : &memory::library_resource()) {}
  ~method_wrapper_state() { reset_member_storage(); }

  method_wrapper_state(const method_wrapper_state&) = delete;
  method_wrapper_state& operator=(const method_wrapper_state&) = delete;
  method_wrapper_state(method_wrapper_state&&) = delete;
  method_wrapper_state& operator=(method_wrapper_state&&) = delete;

  void reset_member_storage() noexcept {
    if (member_heap != nullptr) {
      member_resource->deallocate(member_heap, member_size, member_alignment);
      member_heap = nullptr;
      member_size = 0;
      member_alignment = alignof(std::max_align_t);
    }
  }

  alignas(std::max_align_t) std::array<std::byte, storage_size> storage{};
  void* member_heap = nullptr;
  std::size_t member_size = 0;
  std::size_t member_alignment = alignof(std::max_align_t);
  memory::memory_resource* member_resource = nullptr;
  std::shared_ptr<const method_callable_storage_base> callable{};
};

class method_wrapper final {
public:
  using invoke_fn = bool (*)(void*, method_request_kind, request_payload);
  using destroy_fn = void (*)(void*, memory::memory_resource*) noexcept;

  method_wrapper() noexcept = default;
  explicit method_wrapper(invoke_fn invoke, memory::memory_resource* resource = nullptr)
      : resource_(resource != nullptr ? resource : &memory::library_resource()),
        state_(memory::make<method_wrapper_state>(resource_, resource_)), invoke_(invoke),
        destroy_(&destroy) {}

  method_wrapper(const method_wrapper&) = delete;
  method_wrapper& operator=(const method_wrapper&) = delete;
  method_wrapper(method_wrapper&& other) noexcept
      : resource_(std::exchange(other.resource_, nullptr)),
        state_(std::exchange(other.state_, nullptr)),
        invoke_(std::exchange(other.invoke_, nullptr)),
        destroy_(std::exchange(other.destroy_, nullptr)) {}
  method_wrapper& operator=(method_wrapper&& other) noexcept {
    if (this != &other) {
      reset();
      resource_ = std::exchange(other.resource_, nullptr);
      state_ = std::exchange(other.state_, nullptr);
      invoke_ = std::exchange(other.invoke_, nullptr);
      destroy_ = std::exchange(other.destroy_, nullptr);
    }
    return *this;
  }
  ~method_wrapper() { reset(); }

  [[nodiscard]] bool valid() const noexcept { return state_ != nullptr && invoke_ != nullptr; }

  [[nodiscard]] bool invoke(method_request_kind kind, request_payload payload) const {
    return valid() && invoke_(state_, kind, payload);
  }

  template <typename Member> void store_member(Member member) {
    static_assert(std::is_trivially_copyable_v<Member>);
    auto* state = static_cast<method_wrapper_state*>(state_);
    state->reset_member_storage();
    if constexpr (sizeof(Member) <= method_wrapper_state::storage_size) {
      std::memcpy(state->storage.data(), &member, sizeof(Member));
    } else {
      state->member_heap = state->member_resource->allocate(sizeof(Member), alignof(Member));
      state->member_size = sizeof(Member);
      state->member_alignment = alignof(Member);
      std::memcpy(state->member_heap, &member, sizeof(Member));
    }
  }

  template <typename Member> [[nodiscard]] Member load_member() const noexcept {
    Member member{};
    const auto* state = static_cast<const method_wrapper_state*>(state_);
    const void* source = state->member_heap != nullptr
                             ? state->member_heap
                             : static_cast<const void*>(state->storage.data());
    std::memcpy(&member, source, sizeof(Member));
    return member;
  }

  template <typename Callable> void store_callable(Callable&& callable) {
    using value_type = std::decay_t<Callable>;
    static_cast<method_wrapper_state*>(state_)->callable =
        std::allocate_shared<method_callable_storage_model<value_type>>(
            memory::allocator<method_callable_storage_model<value_type>>{resource_},
            std::forward<Callable>(callable));
  }

  template <typename Callable> [[nodiscard]] const Callable* callable() const noexcept {
    const auto* storage = dynamic_cast<const method_callable_storage_model<Callable>*>(
        static_cast<const method_wrapper_state*>(state_)->callable.get());
    return storage != nullptr ? &storage->value : nullptr;
  }

private:
  static void destroy(void* state, memory::memory_resource* resource) noexcept {
    memory::destroy(static_cast<method_wrapper_state*>(state), resource);
  }

  void reset() noexcept {
    if (state_ != nullptr && destroy_ != nullptr) {
      destroy_(state_, resource_);
    }
    resource_ = nullptr;
    state_ = nullptr;
    invoke_ = nullptr;
    destroy_ = nullptr;
  }

  memory::memory_resource* resource_ = nullptr;
  void* state_ = nullptr;
  invoke_fn invoke_ = nullptr;
  destroy_fn destroy_ = nullptr;
};

/**
 * @brief 底层成员方法反射元信息。
 *
 * 方法调用支持成员函数指针、全局函数指针和 module 级 callable。参数通过 `argument` 传入，
 * 返回值通过 `invoke_result` 区分失败、`void` 成功、拥有值返回和引用返回。
 */
class method_record final {
public:
  method_record() noexcept = default;

  method_record(string_name method_name, method_wrapper::invoke_fn invoke, bool is_const_method,
                memory::memory_resource* resource = nullptr)
      : name_(method_name), const_method_(is_const_method),
        default_arguments_(
            memory::allocator<any>{resource != nullptr ? resource : &memory::library_resource()}),
        parameter_names_(memory::allocator<string_name>{
            resource != nullptr ? resource : &memory::library_resource()}),
        resource_(resource != nullptr ? resource : &memory::library_resource()),
        wrapper_(invoke, resource_) {}

  method_record(const method_record&) = delete;
  method_record& operator=(const method_record&) = delete;
  method_record(method_record&&) noexcept = default;
  method_record& operator=(method_record&&) noexcept = default;
  ~method_record() = default;

  [[nodiscard]] bool valid() const noexcept { return !name_.empty() && wrapper_.valid(); }

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
      metadata_ =
          memory::make_unique<meta::metadata_map>(resource_, meta::make_metadata_map(resource_));
    }
    meta::set_metadata(*metadata_, key, std::move(value));
  }

  [[nodiscard]] meta::type get_owner_type() const noexcept {
    method_type_request request{this};
    return wrapper_.invoke(method_request_kind::owner_type, request_payload{request})
               ? request.result
               : meta::type{};
  }
  [[nodiscard]] meta::type get_return_type() const noexcept {
    method_type_request request{this};
    return wrapper_.invoke(method_request_kind::return_type, request_payload{request})
               ? request.result
               : meta::type{};
  }
  [[nodiscard]] std::size_t parameter_count() const noexcept {
    method_parameter_count_request request{this};
    return wrapper_.invoke(method_request_kind::parameter_count, request_payload{request})
               ? request.result
               : 0;
  }

  [[nodiscard]] std::optional<meta::type> get_parameter_type(std::size_t index) const noexcept {
    method_parameter_type_request request{this, index};
    return wrapper_.invoke(method_request_kind::parameter_type, request_payload{request})
               ? request.result
               : std::nullopt;
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
    if (rank_model_arguments(arguments, {}) == argument_match_rank::exact) {
      return argument_match_rank::exact;
    }

    const argument_match_rank direct_rank = rank_model_arguments(arguments, options);
    if (direct_rank != argument_match_rank::none) {
      return direct_rank;
    }

    vector<argument> completed = complete_positional_arguments(arguments, options);
    return !completed.empty() ? rank_model_arguments(completed, options)
                              : argument_match_rank::none;
  }

  [[nodiscard]] argument_match_rank match_rank(std::span<const meta::value> arguments,
                                               argument_match_options options = {}) const noexcept {
    if (rank_model_value_arguments(arguments, {}) == argument_match_rank::exact) {
      return argument_match_rank::exact;
    }

    const argument_match_rank direct_rank = rank_model_value_arguments(arguments, options);
    if (direct_rank != argument_match_rank::none) {
      return direct_rank;
    }

    vector<meta::value> completed = complete_value_arguments(arguments, options);
    return !completed.empty() ? rank_model_value_arguments(completed, options)
                              : argument_match_rank::none;
  }

  [[nodiscard]] argument_match_rank match_rank(std::span<const named_argument> arguments,
                                               argument_match_options options = {}) const noexcept {
    vector<argument> completed = normalize_named_arguments(arguments, options);
    return !completed.empty() ? rank_model_arguments(completed, options)
                              : argument_match_rank::none;
  }

  [[nodiscard]] bool same_signature(const method_record& other) const noexcept {
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

    invoke_result result = invoke_model(object, mutable_object, completed);
    if (result || result.failure_reason() != invoke_failure_reason::none) {
      return result;
    }
    return invoke_result::failed(invoke_failure_reason::invocation_failed);
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

    invoke_value_result result = invoke_model_value(object, mutable_object, completed);
    if (result || result.failure_reason() != invoke_failure_reason::none) {
      return result;
    }
    return invoke_value_result::failed(invoke_failure_reason::invocation_failed);
  }

  [[nodiscard]] invoke_value_result invoke_runtime_value(const void* object, bool mutable_object,
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

    vector<argument> prepared = prepare_model_arguments(completed, options);
    if (prepared.empty() && parameter_count() > 0) {
      return invoke_result::failed(invoke_failure_reason::argument_mismatch);
    }

    invoke_result result = invoke_model(object, mutable_object, prepared);
    if (result || result.failure_reason() != invoke_failure_reason::none) {
      return result;
    }
    return invoke_result::failed(invoke_failure_reason::invocation_failed);
  }

  [[nodiscard]] any invoke(const void* object, bool mutable_object,
                           std::span<const argument> arguments) const {
    invoke_result result = invoke_result_of(object, mutable_object, arguments);
    return result.has_value() ? std::move(result.mutable_value()) : any{};
  }

  template <typename Member> [[nodiscard]] Member load_member_pointer() const noexcept {
    return wrapper_.template load_member<Member>();
  }

  template <typename Member> void store_member_pointer(Member member) {
    wrapper_.store_member(member);
  }

  template <typename Callable> void store_callable(Callable&& callable) {
    wrapper_.store_callable(std::forward<Callable>(callable));
  }

  template <typename Callable> [[nodiscard]] const Callable* callable() const noexcept {
    return wrapper_.template callable<Callable>();
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
  [[nodiscard]] argument_match_rank
  rank_model_arguments(std::span<const argument> arguments,
                       argument_match_options options) const noexcept {
    method_rank_arguments_request request{this, arguments, options};
    return wrapper_.invoke(method_request_kind::rank_arguments, request_payload{request})
               ? request.result
               : argument_match_rank::none;
  }

  [[nodiscard]] argument_match_rank
  rank_model_value_arguments(std::span<const meta::value> arguments,
                             argument_match_options options) const noexcept {
    method_rank_value_arguments_request request{this, arguments, options};
    return wrapper_.invoke(method_request_kind::rank_value_arguments, request_payload{request})
               ? request.result
               : argument_match_rank::none;
  }

  [[nodiscard]] vector<argument> prepare_model_arguments(std::span<const argument> arguments,
                                                         argument_match_options options) const {
    method_prepare_arguments_request request{this, arguments, options};
    (void)wrapper_.invoke(method_request_kind::prepare_arguments, request_payload{request});
    return std::move(request.result);
  }

  [[nodiscard]] invoke_result invoke_model(const void* object, bool mutable_object,
                                           std::span<const argument> arguments) const {
    method_invoke_request request{this, object, mutable_object, arguments};
    (void)wrapper_.invoke(method_request_kind::invoke, request_payload{request});
    return std::move(request.result);
  }

  [[nodiscard]] invoke_value_result
  invoke_model_value(const void* object, bool mutable_object,
                     std::span<const meta::value> arguments) const {
    method_invoke_value_request request{this, object, mutable_object, arguments};
    (void)wrapper_.invoke(method_request_kind::invoke_value, request_payload{request});
    return std::move(request.result);
  }

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
    if (rank_model_value_arguments(arguments, {}) == argument_match_rank::exact ||
        rank_model_value_arguments(arguments, options) != argument_match_rank::none) {
      return vector<meta::value>{arguments.begin(), arguments.end()};
    }

    return complete_value_arguments(arguments, options);
  }

  [[nodiscard]] vector<argument>
  prepare_positional_arguments(std::span<const argument> arguments,
                               argument_match_options options) const {
    if (rank_model_arguments(arguments, {}) == argument_match_rank::exact) {
      return prepare_model_arguments(arguments, {});
    }

    vector<argument> prepared = prepare_model_arguments(arguments, options);
    if (!prepared.empty() || parameter_count() == 0) {
      return prepared;
    }

    vector<argument> completed = complete_positional_arguments(arguments, options);
    if (completed.empty()) {
      return {};
    }

    return prepare_model_arguments(completed, options);
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
  bool const_method_ = false;
  vector<any> default_arguments_{};
  vector<string_name> parameter_names_{};
  memory::unique_ptr<meta::metadata_map> metadata_{};
  memory::memory_resource* resource_ = &memory::library_resource();
  method_wrapper wrapper_{};
};

/** @brief 同名成员方法的一组重载。 */
class method_set_record final {
public:
  method_set_record() noexcept = default;
  explicit method_set_record(string_name method_name,
                             memory::memory_resource* resource = nullptr) noexcept
      : name_(method_name), overloads_(memory::allocator<method_record>{
                                resource != nullptr ? resource : &memory::library_resource()}) {}

  [[nodiscard]] bool valid() const noexcept {
    if (!name_ || overloads_.empty()) {
      return false;
    }

    for (const method_record& method : overloads_) {
      if (!method.valid() || method.name() != name_) {
        return false;
      }
    }

    return true;
  }

  [[nodiscard]] string_name name() const noexcept { return name_; }
  [[nodiscard]] std::size_t overload_count() const noexcept { return overloads_.size(); }

  [[nodiscard]] const method_record* overload_at(std::size_t index) const noexcept {
    return index < overloads_.size() ? &overloads_[index] : nullptr;
  }

  [[nodiscard]] method_record* mutable_overload_at(std::size_t index) noexcept {
    return index < overloads_.size() ? &overloads_[index] : nullptr;
  }

  [[nodiscard]] const method_record* single() const noexcept {
    return overloads_.size() == 1U ? &overloads_.front() : nullptr;
  }

  [[nodiscard]] const method_record* resolve(std::span<const argument> arguments,
                                             argument_match_options options = {}) const noexcept {
    return match(arguments, options).method;
  }

  [[nodiscard]] method_resolution match(std::span<const argument> arguments,
                                        argument_match_options options = {}) const noexcept {
    const method_record* matched = nullptr;
    argument_match_rank best_rank = argument_match_rank::none;
    for (const method_record& method : overloads_) {
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

  [[nodiscard]] const method_record* resolve(std::span<const meta::value> arguments,
                                             argument_match_options options = {}) const noexcept {
    return match(arguments, options).method;
  }

  [[nodiscard]] method_resolution match(std::span<const meta::value> arguments,
                                        argument_match_options options = {}) const noexcept {
    const method_record* matched = nullptr;
    argument_match_rank best_rank = argument_match_rank::none;
    for (const method_record& method : overloads_) {
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

  [[nodiscard]] invoke_value_result invoke_runtime_value(const void* object, bool mutable_object,
                                                         std::span<const meta::value> arguments,
                                                         argument_match_options options = {}) const;

  [[nodiscard]] invoke_result invoke_result_of(const void* object, bool mutable_object,
                                               std::span<const argument> arguments,
                                               argument_match_options options = {}) const;

  [[nodiscard]] invoke_result invoke_result_of(const void* object, bool mutable_object,
                                               std::span<const named_argument> arguments,
                                               argument_match_options options = {}) const;

  [[nodiscard]] const method_record* resolve(std::span<const named_argument> arguments,
                                             argument_match_options options = {}) const noexcept {
    return match(arguments, options).method;
  }

  [[nodiscard]] method_resolution match(std::span<const named_argument> arguments,
                                        argument_match_options options = {}) const noexcept {
    const method_record* matched = nullptr;
    argument_match_rank best_rank = argument_match_rank::none;
    for (const method_record& method : overloads_) {
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

  [[nodiscard]] bool contains_signature(const method_record& method) const noexcept {
    for (const method_record& overload : overloads_) {
      if (overload.same_signature(method)) {
        return true;
      }
    }

    return false;
  }

  [[nodiscard]] std::optional<std::size_t> add_overload(method_record method) {
    if (!method.valid() || method.name() != name_ || contains_signature(method)) {
      return std::nullopt;
    }

    overloads_.push_back(std::move(method));
    return overloads_.size() - 1U;
  }

private:
  template <typename Arguments>
  [[nodiscard]] method_invocation_resolution
  match_invocable(Arguments arguments, argument_match_options options, const void* object,
                  bool mutable_object) const noexcept {
    const method_record* matched = nullptr;
    argument_match_rank best_rank = argument_match_rank::none;
    bool object_blocked = false;
    bool const_blocked = false;

    for (const method_record& method : overloads_) {
      const argument_match_rank rank = method.match_rank(arguments, options);
      if (rank == argument_match_rank::none) {
        continue;
      }

      if (method.get_owner_type()) {
        if (object == nullptr) {
          object_blocked = true;
          continue;
        }
        if (!method.const_method() && !mutable_object) {
          const_blocked = true;
          continue;
        }
      }

      if (matched != nullptr && rank == best_rank) {
        return {nullptr, method_match_kind::ambiguous, invoke_failure_reason::ambiguous_overload};
      }

      if (matched == nullptr ||
          static_cast<unsigned char>(rank) > static_cast<unsigned char>(best_rank)) {
        matched = &method;
        best_rank = rank;
      }
    }

    if (matched != nullptr) {
      return {matched, method_match_kind_from_rank(best_rank), invoke_failure_reason::none};
    }

    if (const_blocked) {
      return {nullptr, method_match_kind::none, invoke_failure_reason::const_violation};
    }
    if (object_blocked) {
      return {nullptr, method_match_kind::none, invoke_failure_reason::invalid_object};
    }
    return {};
  }

  string_name name_{};
  vector<method_record> overloads_{};
};

} // namespace caors::meta::detail

#endif
