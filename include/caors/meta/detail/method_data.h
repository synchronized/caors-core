// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_METHOD_DATA_H_
#define CAORS_META_DETAIL_METHOD_DATA_H_

#include "caors/core/containers.h"
#include "caors/memory/unique_ptr.h"
#include "caors/meta/metadata.h"
#include "caors/meta/type.h"
#include "caors/meta/value/any_ref.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <functional>
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
  using match_arguments_fn = bool (*)(const method_data& method,
                                      std::span<const any_ref> arguments) noexcept;
  using invoke_fn = any (*)(const method_data& method, const void* object, bool mutable_object,
                            std::span<const any_ref> arguments);

  type_fn get_owner_type = nullptr;
  type_fn get_return_type = nullptr;
  parameter_count_fn parameter_count = nullptr;
  parameter_type_fn get_parameter_type = nullptr;
  match_arguments_fn match_arguments = nullptr;
  invoke_fn invoke = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return get_owner_type != nullptr && get_return_type != nullptr && parameter_count != nullptr &&
           get_parameter_type != nullptr && match_arguments != nullptr && invoke != nullptr;
  }
};

/**
 * @brief 底层成员方法反射元信息。
 *
 * 方法调用只支持已注册的成员函数指针。参数通过 `any_ref` 传入，返回值通过 `any`
 * 拷贝返回；`void` 返回值会得到无效 `any`。
 */
class method_data final {
public:
  static constexpr std::size_t storage_size = sizeof(void*) * 4U;

  method_data() noexcept = default;

  method_data(string_name method_name, method_ops method_operations, bool is_const_method) noexcept
      : name_(method_name), ops_(method_operations), const_method_(is_const_method) {}

  method_data(const method_data& other)
      : name_(other.name_), storage_(other.storage_), ops_(other.ops_),
        const_method_(other.const_method_) {
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

  [[nodiscard]] bool matches_arguments(std::span<const any_ref> arguments) const noexcept {
    return ops_.match_arguments(*this, arguments);
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

  [[nodiscard]] any invoke(const void* object, bool mutable_object,
                           std::span<const any_ref> arguments) const {
    return ops_.invoke(*this, object, mutable_object, arguments);
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

private:
  [[nodiscard]] static const meta::metadata_map& empty_metadata_map() noexcept {
    static const meta::metadata_map empty{
        typename meta::metadata_map::allocator_type{&memory::library_resource()}};
    return empty;
  }

  string_name name_{};
  alignas(std::max_align_t) std::array<std::byte, storage_size> storage_{};
  method_ops ops_{};
  bool const_method_ = false;
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

  [[nodiscard]] const method_data* resolve(std::span<const any_ref> arguments) const noexcept {
    const method_data* matched = nullptr;
    for (const method_data& method : overloads_) {
      if (!method.matches_arguments(arguments)) {
        continue;
      }

      if (matched != nullptr) {
        return nullptr;
      }

      matched = &method;
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

template <typename T> using method_value_type = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename Argument> [[nodiscard]] bool argument_matches(const any_ref& value) noexcept {
  using value_type = method_value_type<Argument>;
  if (!value.template matches_type<value_type>()) {
    return false;
  }

  if constexpr (std::is_lvalue_reference_v<Argument> &&
                !std::is_const_v<std::remove_reference_t<Argument>>) {
    return value.writable();
  } else {
    return true;
  }
}

template <typename Argument> decltype(auto) argument_value(const any_ref& value) noexcept {
  using value_type = method_value_type<Argument>;
  if constexpr (std::is_lvalue_reference_v<Argument> &&
                !std::is_const_v<std::remove_reference_t<Argument>>) {
    return *value.template try_get_mutable<value_type>();
  } else {
    return *value.template try_get<value_type>();
  }
}

template <typename... Arguments>
[[nodiscard]] bool arguments_match(std::span<const any_ref> arguments) noexcept {
  if (arguments.size() != sizeof...(Arguments)) {
    return false;
  }

  std::size_t index = 0;
  return ((argument_matches<Arguments>(arguments[index++])) && ...);
}

template <typename Class, typename Return, typename... Arguments, std::size_t... Index>
[[nodiscard]] any invoke_member_impl(Return (Class::*member)(Arguments...), Class& object,
                                     std::span<const any_ref> arguments,
                                     std::index_sequence<Index...>) {
  if constexpr (std::is_void_v<Return>) {
    std::invoke(member, object, argument_value<Arguments>(arguments[Index])...);
    return {};
  } else {
    static_assert(std::is_copy_constructible_v<method_value_type<Return>>,
                  "Reflected method return values must be copy constructible");
    decltype(auto) result =
        std::invoke(member, object, argument_value<Arguments>(arguments[Index])...);
    return any::from(result);
  }
}

template <typename Class, typename Return, typename... Arguments, std::size_t... Index>
[[nodiscard]] any invoke_member_impl(Return (Class::*member)(Arguments...) const,
                                     const Class& object, std::span<const any_ref> arguments,
                                     std::index_sequence<Index...>) {
  if constexpr (std::is_void_v<Return>) {
    std::invoke(member, object, argument_value<Arguments>(arguments[Index])...);
    return {};
  } else {
    static_assert(std::is_copy_constructible_v<method_value_type<Return>>,
                  "Reflected method return values must be copy constructible");
    decltype(auto) result =
        std::invoke(member, object, argument_value<Arguments>(arguments[Index])...);
    return any::from(result);
  }
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] any invoke_member_method(const method_data& method, const void* object,
                                       bool mutable_object, std::span<const any_ref> arguments) {
  static_assert((!std::is_rvalue_reference_v<Arguments> && ...),
                "Reflected method rvalue reference parameters are not supported");
  if (object == nullptr || !mutable_object || !arguments_match<Arguments...>(arguments)) {
    return {};
  }

  auto member = method.template load_member_pointer<Return (Class::*)(Arguments...)>();
  auto* instance = const_cast<Class*>(static_cast<const Class*>(object));
  return invoke_member_impl(member, *instance, arguments, std::index_sequence_for<Arguments...>{});
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] any invoke_const_member_method(const method_data& method, const void* object,
                                             bool /*mutable_object*/,
                                             std::span<const any_ref> arguments) {
  static_assert((!std::is_rvalue_reference_v<Arguments> && ...),
                "Reflected method rvalue reference parameters are not supported");
  if (object == nullptr || !arguments_match<Arguments...>(arguments)) {
    return {};
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
[[nodiscard]] bool method_arguments_match(const method_data&,
                                          std::span<const any_ref> arguments) noexcept {
  return arguments_match<Arguments...>(arguments);
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] method_ops member_method_ops() noexcept {
  return method_ops{
      &method_owner_type<Class>,
      &method_return_type<Return>,
      &method_parameter_count<Arguments...>,
      &method_parameter_type<Arguments...>,
      &method_arguments_match<Arguments...>,
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
      &method_arguments_match<Arguments...>,
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

} // namespace caors::meta::detail

#endif
