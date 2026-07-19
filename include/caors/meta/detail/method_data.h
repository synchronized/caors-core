// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_METHOD_DATA_H_
#define CAORS_META_DETAIL_METHOD_DATA_H_

#include "caors/core/containers.h"
#include "caors/memory/resource.h"
#include "caors/memory/unique_ptr.h"
#include "caors/meta/detail/argument_access.h"
#include "caors/meta/metadata.h"
#include "caors/meta/method.h"

#include <array>
#include <cstddef>
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

  type_fn get_owner_type = nullptr;
  type_fn get_return_type = nullptr;
  parameter_count_fn parameter_count = nullptr;
  parameter_type_fn get_parameter_type = nullptr;
  rank_arguments_fn rank_arguments = nullptr;
  prepare_arguments_fn prepare_arguments = nullptr;
  invoke_fn invoke = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return get_owner_type != nullptr && get_return_type != nullptr && parameter_count != nullptr &&
           get_parameter_type != nullptr && rank_arguments != nullptr &&
           prepare_arguments != nullptr && invoke != nullptr;
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
    vector<argument> completed = prepare_positional_arguments(arguments, options);
    if (completed.empty() && parameter_count() > 0) {
      return invoke_result::failed();
    }

    return ops_.invoke(*this, object, mutable_object, completed);
  }

  [[nodiscard]] invoke_result invoke_result_of(const void* object, bool mutable_object,
                                               std::span<const named_argument> arguments,
                                               argument_match_options options = {}) const {
    vector<argument> completed = normalize_named_arguments(arguments, options);
    if (completed.empty()) {
      return invoke_result::failed();
    }

    vector<argument> prepared = ops_.prepare_arguments(*this, completed, options);
    if (prepared.empty() && parameter_count() > 0) {
      return invoke_result::failed();
    }

    return ops_.invoke(*this, object, mutable_object, prepared);
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
    const method_data* matched = nullptr;
    argument_match_rank best_rank = argument_match_rank::none;
    for (const method_data& method : overloads_) {
      const argument_match_rank rank = method.match_rank(arguments, options);
      if (rank == argument_match_rank::none) {
        continue;
      }

      if (matched != nullptr && rank == best_rank) {
        return nullptr;
      }

      if (matched == nullptr ||
          static_cast<unsigned char>(rank) > static_cast<unsigned char>(best_rank)) {
        matched = &method;
        best_rank = rank;
      }
    }

    return matched;
  }

  [[nodiscard]] const method_data* resolve(std::span<const named_argument> arguments,
                                           argument_match_options options = {}) const noexcept {
    const method_data* matched = nullptr;
    argument_match_rank best_rank = argument_match_rank::none;
    for (const method_data& method : overloads_) {
      const argument_match_rank rank = method.match_rank(arguments, options);
      if (rank == argument_match_rank::none) {
        continue;
      }

      if (matched != nullptr && rank == best_rank) {
        return nullptr;
      }

      if (matched == nullptr ||
          static_cast<unsigned char>(rank) > static_cast<unsigned char>(best_rank)) {
        matched = &method;
        best_rank = rank;
      }
    }

    return matched;
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

template <typename T> using method_value_type = argument_value_type<T>;

template <typename Return, typename Result>
[[nodiscard]] invoke_result make_method_invoke_result(Result&& result) {
  if constexpr (std::is_lvalue_reference_v<Return>) {
    return invoke_result::reference(any_ref::from(std::forward<Result>(result)));
  } else {
    using value_type = method_value_type<Return>;
    if constexpr (std::is_copy_constructible_v<value_type>) {
      return invoke_result::value(any::from(result));
    } else if constexpr (std::is_move_constructible_v<value_type>) {
      auto* value = memory::make<value_type>(&memory::library_resource(), std::move(result));
      return invoke_result::value(any::take_ownership(
          make_owned_value(meta::resolve<value_type>(), value, &memory::library_resource(),
                           [](memory::memory_resource* resource, void* instance) noexcept {
                             memory::destroy(static_cast<value_type*>(instance), resource);
                           })));
    } else {
      return invoke_result::failed();
    }
  }
}

template <typename Class, typename Return, typename... Arguments, std::size_t... Index>
[[nodiscard]] invoke_result invoke_member_impl(Return (Class::*member)(Arguments...), Class& object,
                                               std::span<const argument> arguments,
                                               std::index_sequence<Index...>) {
  if constexpr (std::is_void_v<Return>) {
    std::invoke(member, object, argument_value<Arguments>(arguments[Index])...);
    return invoke_result::void_value();
  } else {
    decltype(auto) result =
        std::invoke(member, object, argument_value<Arguments>(arguments[Index])...);
    return make_method_invoke_result<Return>(result);
  }
}

template <typename Class, typename Return, typename... Arguments, std::size_t... Index>
[[nodiscard]] invoke_result
invoke_member_impl(Return (Class::*member)(Arguments...) const, const Class& object,
                   std::span<const argument> arguments, std::index_sequence<Index...>) {
  if constexpr (std::is_void_v<Return>) {
    std::invoke(member, object, argument_value<Arguments>(arguments[Index])...);
    return invoke_result::void_value();
  } else {
    decltype(auto) result =
        std::invoke(member, object, argument_value<Arguments>(arguments[Index])...);
    return make_method_invoke_result<Return>(result);
  }
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] invoke_result invoke_member_method(const method_data& method, const void* object,
                                                 bool mutable_object,
                                                 std::span<const argument> arguments) {
  if (object == nullptr || !mutable_object || !arguments_match<Arguments...>(arguments)) {
    return invoke_result::failed();
  }

  auto member = method.template load_member_pointer<Return (Class::*)(Arguments...)>();
  auto* instance = const_cast<Class*>(static_cast<const Class*>(object));
  return invoke_member_impl(member, *instance, arguments, std::index_sequence_for<Arguments...>{});
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] invoke_result invoke_const_member_method(const method_data& method,
                                                       const void* object, bool /*mutable_object*/,
                                                       std::span<const argument> arguments) {
  if (object == nullptr || !arguments_match<Arguments...>(arguments)) {
    return invoke_result::failed();
  }

  auto member = method.template load_member_pointer<Return (Class::*)(Arguments...) const>();
  const auto* instance = static_cast<const Class*>(object);
  return invoke_member_impl(member, *instance, arguments, std::index_sequence_for<Arguments...>{});
}

template <typename Class> [[nodiscard]] meta::type method_owner_type(const method_data&) noexcept {
  return meta::resolve<Class>();
}

template <typename Return>
[[nodiscard]] meta::type method_return_type(const method_data&) noexcept {
  if constexpr (std::is_void_v<method_value_type<Return>>) {
    return {};
  } else {
    return meta::resolve<method_value_type<Return>>();
  }
}

template <typename... Arguments>
[[nodiscard]] std::size_t method_parameter_count(const method_data&) noexcept {
  return sizeof...(Arguments);
}

template <typename... Arguments>
[[nodiscard]] std::optional<meta::type> method_parameter_type(const method_data&,
                                                              std::size_t index) noexcept {
  if constexpr (sizeof...(Arguments) == 0) {
    return std::nullopt;
  } else {
    if (index >= sizeof...(Arguments)) {
      return std::nullopt;
    }

    std::array<meta::type, sizeof...(Arguments)> types{
        meta::resolve<method_value_type<Arguments>>()...};
    if (!types[index]) {
      return std::nullopt;
    }

    return types[index];
  }
}

template <typename... Arguments>
[[nodiscard]] argument_match_rank method_arguments_rank(const method_data&,
                                                        std::span<const argument> arguments,
                                                        argument_match_options options) noexcept {
  return arguments_rank<Arguments...>(arguments, options);
}

template <typename... Arguments>
[[nodiscard]] vector<argument> method_prepare_arguments(const method_data&,
                                                        std::span<const argument> arguments,
                                                        argument_match_options options) {
  return prepare_arguments<Arguments...>(arguments, options);
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] method_ops member_method_ops() noexcept {
  return method_ops{
      &method_owner_type<Class>,
      &method_return_type<Return>,
      &method_parameter_count<Arguments...>,
      &method_parameter_type<Arguments...>,
      &method_arguments_rank<Arguments...>,
      &method_prepare_arguments<Arguments...>,
      &invoke_member_method<Class, Return, Arguments...>,
  };
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] method_ops const_member_method_ops() noexcept {
  return method_ops{
      &method_owner_type<Class>,
      &method_return_type<Return>,
      &method_parameter_count<Arguments...>,
      &method_parameter_type<Arguments...>,
      &method_arguments_rank<Arguments...>,
      &method_prepare_arguments<Arguments...>,
      &invoke_const_member_method<Class, Return, Arguments...>,
  };
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] method_data make_method_data(std::string_view name,
                                           Return (Class::*member)(Arguments...)) {
  method_data method{
      string_name{name},
      member_method_ops<Class, Return, Arguments...>(),
      false,
  };
  method.store_member_pointer(member);
  return method;
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] method_data make_method_data(std::string_view name,
                                           Return (Class::*member)(Arguments...) const) {
  method_data method{
      string_name{name},
      const_member_method_ops<Class, Return, Arguments...>(),
      true,
  };
  method.store_member_pointer(member);
  return method;
}

template <typename Return, typename... Arguments> struct function_signature {
  using return_type = Return;
};

template <typename Signature> struct callable_signature;

template <typename Class, typename Return, typename... Arguments>
struct callable_signature<Return (Class::*)(Arguments...) const> {
  using type = function_signature<Return, Arguments...>;
};

template <typename Class, typename Return, typename... Arguments>
struct callable_signature<Return (Class::*)(Arguments...) const noexcept> {
  using type = function_signature<Return, Arguments...>;
};

template <typename Class, typename Return, typename... Arguments>
struct callable_signature<Return (Class::*)(Arguments...)> {
  using type = function_signature<Return, Arguments...>;
};

template <typename Class, typename Return, typename... Arguments>
struct callable_signature<Return (Class::*)(Arguments...) noexcept> {
  using type = function_signature<Return, Arguments...>;
};

template <typename Callable>
using callable_signature_t =
    typename callable_signature<decltype(&std::decay_t<Callable>::operator())>::type;

template <typename Return, typename... Arguments, std::size_t... Index>
[[nodiscard]] invoke_result invoke_global_impl(Return (*function)(Arguments...),
                                               std::span<const argument> arguments,
                                               std::index_sequence<Index...>) {
  if constexpr (std::is_void_v<Return>) {
    std::invoke(function, argument_value<Arguments>(arguments[Index])...);
    return invoke_result::void_value();
  } else {
    decltype(auto) result = std::invoke(function, argument_value<Arguments>(arguments[Index])...);
    return make_method_invoke_result<Return>(result);
  }
}

template <typename Return, typename... Arguments>
[[nodiscard]] invoke_result invoke_global_method(const method_data& method, const void*, bool,
                                                 std::span<const argument> arguments) {
  if (!arguments_match<Arguments...>(arguments)) {
    return invoke_result::failed();
  }

  auto function = method.template load_member_pointer<Return (*)(Arguments...)>();
  if (function == nullptr) {
    return invoke_result::failed();
  }

  return invoke_global_impl(function, arguments, std::index_sequence_for<Arguments...>{});
}

template <typename Callable, typename Return, typename... Arguments, std::size_t... Index>
[[nodiscard]] invoke_result invoke_global_callable_impl(const Callable& callable,
                                                        std::span<const argument> arguments,
                                                        std::index_sequence<Index...>) {
  if constexpr (std::is_void_v<Return>) {
    std::invoke(callable, argument_value<Arguments>(arguments[Index])...);
    return invoke_result::void_value();
  } else {
    decltype(auto) result = std::invoke(callable, argument_value<Arguments>(arguments[Index])...);
    return make_method_invoke_result<Return>(result);
  }
}

template <typename Callable, typename Return, typename... Arguments>
[[nodiscard]] invoke_result invoke_global_callable_method(const method_data& method, const void*,
                                                          bool,
                                                          std::span<const argument> arguments) {
  if (!arguments_match<Arguments...>(arguments)) {
    return invoke_result::failed();
  }

  const auto* callable = method.template callable<Callable>();
  if (callable == nullptr) {
    return invoke_result::failed();
  }

  return invoke_global_callable_impl<Callable, Return, Arguments...>(
      *callable, arguments, std::index_sequence_for<Arguments...>{});
}

template <typename Return, typename... Arguments>
[[nodiscard]] meta::type global_method_owner_type(const method_data&) noexcept {
  return {};
}

template <typename Return, typename... Arguments>
[[nodiscard]] method_ops global_method_ops() noexcept {
  return method_ops{
      &global_method_owner_type<Return, Arguments...>,
      &method_return_type<Return>,
      &method_parameter_count<Arguments...>,
      &method_parameter_type<Arguments...>,
      &method_arguments_rank<Arguments...>,
      &method_prepare_arguments<Arguments...>,
      &invoke_global_method<Return, Arguments...>,
  };
}

template <typename Return, typename... Arguments>
[[nodiscard]] method_data make_global_method_data(std::string_view name,
                                                  Return (*function)(Arguments...)) {
  method_data method{
      string_name{name},
      global_method_ops<Return, Arguments...>(),
      true,
  };
  method.store_member_pointer(function);
  return method;
}

template <typename Callable, typename Signature> struct global_callable_method_factory;

template <typename Callable, typename Return, typename... Arguments>
struct global_callable_method_factory<Callable, function_signature<Return, Arguments...>> {
  [[nodiscard]] static method_ops ops() noexcept {
    return method_ops{
        &global_method_owner_type<Return, Arguments...>,
        &method_return_type<Return>,
        &method_parameter_count<Arguments...>,
        &method_parameter_type<Arguments...>,
        &method_arguments_rank<Arguments...>,
        &method_prepare_arguments<Arguments...>,
        &invoke_global_callable_method<Callable, Return, Arguments...>,
    };
  }

  template <typename Value>
  [[nodiscard]] static method_data make(std::string_view name, Value&& callable) {
    method_data method{
        string_name{name},
        ops(),
        true,
    };
    method.store_callable(std::forward<Value>(callable));
    return method;
  }
};

template <typename Callable>
[[nodiscard]] method_data make_global_callable_method_data(std::string_view name,
                                                           Callable&& callable) {
  using value_type = std::decay_t<Callable>;
  using signature = callable_signature_t<value_type>;
  return global_callable_method_factory<value_type, signature>::make(
      name, std::forward<Callable>(callable));
}

} // namespace caors::meta::detail

#endif
