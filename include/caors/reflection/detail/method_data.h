// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_DETAIL_METHOD_DATA_H_
#define CAORS_REFLECTION_DETAIL_METHOD_DATA_H_

#include "caors/core/containers.h"
#include "caors/meta.h"
#include "caors/meta/detail/type_descriptor_model.h"
#include "caors/value/any_ref.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <functional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::reflection::detail {

struct method_data;

using method_invoke_fn = any (*)(const method_data& method, const void* object, bool mutable_object,
                                 std::span<const any_ref> arguments);

/**
 * @brief 底层成员方法反射元信息。
 *
 * 方法调用只支持已注册的成员函数指针。参数通过 `any_ref` 传入，返回值通过 `any`
 * 拷贝返回；`void` 返回值会得到无效 `any`。
 */
struct method_data {
  static constexpr std::size_t storage_size = sizeof(void*) * 4U;

  string_name name{};
  type_id return_type{};
  const meta::type_descriptor* return_descriptor = nullptr;
  vector<type_id> parameter_types{};
  vector<const meta::type_descriptor*> parameter_descriptors{};
  alignas(std::max_align_t) std::array<std::byte, storage_size> storage{};
  method_invoke_fn invoke = nullptr;
  bool const_method = false;

  [[nodiscard]] bool valid() const noexcept {
    return !name.empty() && invoke != nullptr &&
           parameter_types.size() == parameter_descriptors.size();
  }
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

template <typename Member>
[[nodiscard]] Member load_member_pointer(const method_data& method) noexcept {
  Member member{};
  std::memcpy(&member, method.storage.data(), sizeof(Member));
  return member;
}

template <typename Member> void store_member_pointer(method_data& method, Member member) noexcept {
  static_assert(std::is_trivially_copyable_v<Member>, "Member pointers must be trivially copyable");
  static_assert(sizeof(Member) <= method_data::storage_size,
                "Member pointer is too large for reflection method storage");
  std::memcpy(method.storage.data(), &member, sizeof(Member));
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

  auto member = load_member_pointer<Return (Class::*)(Arguments...)>(method);
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

  auto member = load_member_pointer<Return (Class::*)(Arguments...) const>(method);
  const auto* instance = static_cast<const Class*>(object);
  return invoke_member_impl(member, *instance, arguments, std::index_sequence_for<Arguments...>{});
}

template <typename Return>
[[nodiscard]] const meta::type_descriptor* return_descriptor_ptr() noexcept {
  if constexpr (std::is_void_v<method_value_type<Return>>) {
    return nullptr;
  } else {
    return meta::detail::descriptor_ptr_of<method_value_type<Return>>();
  }
}

template <typename Return> [[nodiscard]] type_id return_type_id() noexcept {
  if constexpr (std::is_void_v<method_value_type<Return>>) {
    return type_id::invalid();
  } else {
    return meta::detail::descriptor_type_id<method_value_type<Return>>();
  }
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] method_data make_method_data(std::string_view name,
                                           Return (Class::*member)(Arguments...)) {
  method_data method{
      string_name{name},
      return_type_id<Return>(),
      return_descriptor_ptr<Return>(),
      vector<type_id>{meta::detail::descriptor_type_id<method_value_type<Arguments>>()...},
      vector<const meta::type_descriptor*>{
          meta::detail::descriptor_ptr_of<method_value_type<Arguments>>()...},
      {},
      &invoke_member_method<Class, Return, Arguments...>,
      false,
  };
  store_member_pointer(method, member);
  return method;
}

template <typename Class, typename Return, typename... Arguments>
[[nodiscard]] method_data make_method_data(std::string_view name,
                                           Return (Class::*member)(Arguments...) const) {
  method_data method{
      string_name{name},
      return_type_id<Return>(),
      return_descriptor_ptr<Return>(),
      vector<type_id>{meta::detail::descriptor_type_id<method_value_type<Arguments>>()...},
      vector<const meta::type_descriptor*>{
          meta::detail::descriptor_ptr_of<method_value_type<Arguments>>()...},
      {},
      &invoke_const_member_method<Class, Return, Arguments...>,
      true,
  };
  store_member_pointer(method, member);
  return method;
}

} // namespace caors::reflection::detail

#endif
