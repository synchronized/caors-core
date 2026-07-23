// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_PROPERTY_MODEL_H_
#define CAORS_META_DETAIL_PROPERTY_MODEL_H_

#include "caors/meta/detail/property_data.h"

namespace caors::meta::detail {

template <typename T> [[nodiscard]] bool property_type_matches_name(std::string_view name) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  const meta::type registered = caors::meta::find<type>();
  return registered && !registered.name().empty() && registered.name() == name;
}

template <typename Class>
[[nodiscard]] meta::type member_property_owner_type(const property_data&) noexcept {
  return meta::resolve<Class>();
}

template <typename Property>
[[nodiscard]] meta::type member_property_type(const property_data&) noexcept {
  using property_type = std::remove_cv_t<Property>;
  return meta::resolve<property_type>();
}

[[nodiscard]] inline const void* member_property_data(const property_data& property,
                                                      const void* object) noexcept {
  return property.member_data(object);
}

[[nodiscard]] inline void* member_property_mutable_data(const property_data& property,
                                                        void* object) noexcept {
  return property.member_mutable_data(object);
}

template <typename Property>
[[nodiscard]] any member_property_get_value(const property_data& property, const void* object) {
  using property_type = std::remove_cv_t<Property>;
  return property.member_get_value(meta::resolve<property_type>(), object);
}

template <typename Property>
[[nodiscard]] any_ref member_property_get_const_ref(const property_data& property,
                                                    const void* object) {
  using property_type = std::remove_cv_t<Property>;
  return property.member_get_ref(meta::resolve<property_type>(), object);
}

template <typename Property>
[[nodiscard]] any_ref member_property_get_mutable_ref(const property_data& property, void* object) {
  using property_type = std::remove_cv_t<Property>;
  return property.member_get_ref(meta::resolve<property_type>(), object);
}

template <typename Property>
bool member_property_set_value(const property_data& property, void* object, const any& source) {
  return property.member_try_set_value(object, source);
}

template <typename Class, typename Property>
[[nodiscard]] property_ops member_property_ops() noexcept {
  return property_ops{
      &member_property_owner_type<Class>,
      &member_property_type<Property>,
      &member_property_data,
      &member_property_mutable_data,
      &member_property_get_value<Property>,
      &member_property_get_const_ref<Property>,
      &member_property_get_mutable_ref<Property>,
      &member_property_set_value<Property>,
  };
}

template <typename Getter> struct const_member_getter_traits;

template <typename Class, typename Return>
struct const_member_getter_traits<Return (Class::*)() const> {
  using class_type = Class;
  using return_type = Return;
  using value_type = std::remove_cv_t<std::remove_reference_t<Return>>;
  using pointer_type = Return (Class::*)() const;
};

template <typename Class, typename Return>
struct const_member_getter_traits<Return (Class::*)() const noexcept> {
  using class_type = Class;
  using return_type = Return;
  using value_type = std::remove_cv_t<std::remove_reference_t<Return>>;
  using pointer_type = Return (Class::*)() const noexcept;
};

template <typename Setter> struct member_setter_traits;

template <typename Class, typename Return, typename Argument>
struct member_setter_traits<Return (Class::*)(Argument)> {
  using class_type = Class;
  using return_type = Return;
  using argument_type = Argument;
  using value_type = std::remove_cv_t<std::remove_reference_t<Argument>>;
  using pointer_type = Return (Class::*)(Argument);
};

template <typename Class, typename Return, typename Argument>
struct member_setter_traits<Return (Class::*)(Argument) noexcept> {
  using class_type = Class;
  using return_type = Return;
  using argument_type = Argument;
  using value_type = std::remove_cv_t<std::remove_reference_t<Argument>>;
  using pointer_type = Return (Class::*)(Argument) noexcept;
};

template <typename Class>
[[nodiscard]] meta::type accessor_property_owner_type(const property_data&) noexcept {
  return meta::resolve<Class>();
}

template <typename Getter>
[[nodiscard]] meta::type accessor_property_type(const property_data&) noexcept {
  using traits_type = const_member_getter_traits<Getter>;
  return meta::resolve<typename traits_type::value_type>();
}

template <typename Getter>
decltype(auto) invoke_property_getter(const property_data& property, const void* object) {
  using traits_type = const_member_getter_traits<Getter>;
  auto getter = property.template load_pointer<typename traits_type::pointer_type>(0);
  return std::invoke(getter, *static_cast<const typename traits_type::class_type*>(object));
}

template <typename Getter>
[[nodiscard]] const void* accessor_property_data(const property_data& property,
                                                 const void* object) noexcept {
  using traits_type = const_member_getter_traits<Getter>;
  using return_type = typename traits_type::return_type;
  if (object == nullptr) {
    return nullptr;
  }

  if constexpr (std::is_lvalue_reference_v<return_type>) {
    decltype(auto) value = invoke_property_getter<Getter>(property, object);
    return std::addressof(value);
  } else {
    return nullptr;
  }
}

template <typename Getter>
[[nodiscard]] void* accessor_property_mutable_data(const property_data& property,
                                                   void* object) noexcept {
  using traits_type = const_member_getter_traits<Getter>;
  using return_type = typename traits_type::return_type;
  if (object == nullptr) {
    return nullptr;
  }

  if constexpr (std::is_lvalue_reference_v<return_type> &&
                !std::is_const_v<std::remove_reference_t<return_type>>) {
    decltype(auto) value = invoke_property_getter<Getter>(property, object);
    return std::addressof(value);
  } else {
    return nullptr;
  }
}

template <typename Getter>
[[nodiscard]] any accessor_property_get_value(const property_data& property, const void* object) {
  using traits_type = const_member_getter_traits<Getter>;
  using value_type = typename traits_type::value_type;
  static_assert(std::is_copy_constructible_v<value_type>,
                "Reflected property getter values must be copy constructible");
  if (object == nullptr) {
    return {};
  }

  decltype(auto) value = invoke_property_getter<Getter>(property, object);
  return any::from(value);
}

template <typename Getter>
[[nodiscard]] any_ref accessor_property_get_const_ref(const property_data& property,
                                                      const void* object) {
  using traits_type = const_member_getter_traits<Getter>;
  using return_type = typename traits_type::return_type;
  using value_type = typename traits_type::value_type;
  if (object == nullptr) {
    return {};
  }

  if constexpr (std::is_lvalue_reference_v<return_type>) {
    decltype(auto) value = invoke_property_getter<Getter>(property, object);
    return any_ref::from_raw(meta::resolve<value_type>(), std::addressof(value), false);
  } else {
    return {};
  }
}

template <typename Getter>
[[nodiscard]] any_ref accessor_property_get_mutable_ref(const property_data& property,
                                                        void* object) {
  using traits_type = const_member_getter_traits<Getter>;
  using return_type = typename traits_type::return_type;
  using value_type = typename traits_type::value_type;
  if (object == nullptr) {
    return {};
  }

  if constexpr (std::is_lvalue_reference_v<return_type> &&
                !std::is_const_v<std::remove_reference_t<return_type>>) {
    decltype(auto) value = invoke_property_getter<Getter>(property, object);
    return any_ref::from_raw(meta::resolve<value_type>(), std::addressof(value), true);
  } else {
    return {};
  }
}

template <typename Getter>
bool readonly_accessor_property_set_value(const property_data&, void*, const any&) {
  return false;
}

template <typename Getter, typename Setter>
bool accessor_property_set_value(const property_data& property, void* object, const any& source) {
  using setter_traits = member_setter_traits<Setter>;
  using return_type = typename setter_traits::return_type;
  using argument_type = typename setter_traits::argument_type;
  using value_type = typename setter_traits::value_type;
  static_assert(!std::is_rvalue_reference_v<argument_type>,
                "Reflected property setters do not support rvalue reference arguments");
  static_assert(!std::is_lvalue_reference_v<argument_type> ||
                    std::is_const_v<std::remove_reference_t<argument_type>>,
                "Reflected property setters do not support non-const lvalue reference arguments");
  static_assert(std::is_void_v<return_type> || std::is_same_v<return_type, bool>,
                "Reflected property setters must return void or bool");
  if (object == nullptr || !source.template matches_type<value_type>()) {
    return false;
  }

  const value_type* value = source.template try_get<value_type>();
  if (value == nullptr) {
    return false;
  }

  auto setter =
      property.template load_pointer<typename setter_traits::pointer_type>(sizeof(Getter));
  if constexpr (std::is_void_v<return_type>) {
    std::invoke(setter, *static_cast<typename setter_traits::class_type*>(object), *value);
    return true;
  } else {
    return std::invoke(setter, *static_cast<typename setter_traits::class_type*>(object), *value);
  }
}

template <typename Class, typename Getter>
[[nodiscard]] property_ops readonly_accessor_property_ops() noexcept {
  return property_ops{
      &accessor_property_owner_type<Class>,
      &accessor_property_type<Getter>,
      &accessor_property_data<Getter>,
      &accessor_property_mutable_data<Getter>,
      &accessor_property_get_value<Getter>,
      &accessor_property_get_const_ref<Getter>,
      &accessor_property_get_mutable_ref<Getter>,
      &readonly_accessor_property_set_value<Getter>,
  };
}

template <typename Class, typename Getter, typename Setter>
[[nodiscard]] property_ops accessor_property_ops() noexcept {
  return property_ops{
      &accessor_property_owner_type<Class>,
      &accessor_property_type<Getter>,
      &accessor_property_data<Getter>,
      &accessor_property_mutable_data<Getter>,
      &accessor_property_get_value<Getter>,
      &accessor_property_get_const_ref<Getter>,
      &accessor_property_get_mutable_ref<Getter>,
      &accessor_property_set_value<Getter, Setter>,
  };
}

/** @brief 创建属性反射元信息。 */
template <typename Class, typename Property>
[[nodiscard]] property_data make_property_data(std::string_view name, std::string_view type_name,
                                               std::size_t offset) {
  using property_type = std::remove_cv_t<Property>;
  if (!meta::resolve<property_type>()) {
    return {};
  }

  if (!type_name.empty() && !property_type_matches_name<property_type>(type_name)) {
    return {};
  }

  property_data property{
      string_name{name},
      offset,
      !std::is_const_v<Property>,
      property_storage_kind::member,
      member_property_ops<Class, Property>(),
  };

  return property;
}

template <typename Property>
[[nodiscard]] meta::type global_property_owner_type(const property_data&) noexcept {
  return {};
}

template <typename Property>
[[nodiscard]] meta::type global_property_type(const property_data&) noexcept {
  using property_type = std::remove_cv_t<Property>;
  return meta::resolve<property_type>();
}

template <typename Property>
[[nodiscard]] const void* global_property_data(const property_data& property,
                                               const void*) noexcept {
  return property.template load_pointer<Property*>(0);
}

template <typename Property>
[[nodiscard]] void* global_property_mutable_data(const property_data& property, void*) noexcept {
  if constexpr (std::is_const_v<Property>) {
    return nullptr;
  } else {
    return property.template load_pointer<Property*>(0);
  }
}

template <typename Property>
[[nodiscard]] any global_property_get_value(const property_data& property, const void*) {
  const auto* value = property.template load_pointer<Property*>(0);
  return value != nullptr ? any::from(*value) : any{};
}

template <typename Property>
[[nodiscard]] any_ref global_property_get_const_ref(const property_data& property, const void*) {
  using property_type = std::remove_cv_t<Property>;
  const auto* value = property.template load_pointer<Property*>(0);
  return value != nullptr ? any_ref::from_raw(meta::resolve<property_type>(), value, false)
                          : any_ref{};
}

template <typename Property>
[[nodiscard]] any_ref global_property_get_mutable_ref(const property_data& property, void*) {
  if constexpr (std::is_const_v<Property>) {
    return {};
  } else {
    using property_type = std::remove_cv_t<Property>;
    auto* value = property.template load_pointer<Property*>(0);
    return value != nullptr ? any_ref::from_raw(meta::resolve<property_type>(), value, true)
                            : any_ref{};
  }
}

template <typename Property>
bool global_property_set_value(const property_data& property, void*, const any& source) {
  if constexpr (std::is_const_v<Property>) {
    return false;
  } else {
    using property_type = std::remove_cv_t<Property>;
    auto* value = property.template load_pointer<Property*>(0);
    if (value == nullptr || !source.template matches_type<property_type>()) {
      return false;
    }

    const property_type* source_value = source.template try_get<property_type>();
    if (source_value == nullptr) {
      return false;
    }

    if constexpr (std::is_assignable_v<property_type&, const property_type&>) {
      *value = *source_value;
      return true;
    } else {
      return false;
    }
  }
}

template <typename Property> [[nodiscard]] property_ops global_property_ops() noexcept {
  return property_ops{
      &global_property_owner_type<Property>,
      &global_property_type<Property>,
      &global_property_data<Property>,
      &global_property_mutable_data<Property>,
      &global_property_get_value<Property>,
      &global_property_get_const_ref<Property>,
      &global_property_get_mutable_ref<Property>,
      &global_property_set_value<Property>,
  };
}

template <typename Property>
[[nodiscard]] property_data make_global_property_data(std::string_view name, Property* pointer) {
  using property_type = std::remove_cv_t<Property>;
  if (pointer == nullptr || !meta::resolve<property_type>()) {
    return {};
  }

  property_data property{
      string_name{name},
      0,
      !std::is_const_v<Property>,
      property_storage_kind::global,
      global_property_ops<Property>(),
  };
  property.store_pointer(pointer, 0);
  return property;
}

template <typename Class, typename Getter>
[[nodiscard]] property_data make_readonly_property_data(std::string_view name, Getter getter) {
  using getter_traits = const_member_getter_traits<Getter>;
  static_assert(std::is_same_v<typename getter_traits::class_type, Class>,
                "Property getter owner must match builder type");
  using value_type = typename getter_traits::value_type;
  if (!meta::resolve<value_type>()) {
    return {};
  }

  property_data property{
      string_name{name},
      0,
      false,
      property_storage_kind::accessor,
      readonly_accessor_property_ops<Class, Getter>(),
  };
  property.store_getter_pointer(getter);
  return property;
}

template <typename Class, typename Getter, typename Setter>
[[nodiscard]] property_data make_accessor_property_data(std::string_view name, Getter getter,
                                                        Setter setter) {
  using getter_traits = const_member_getter_traits<Getter>;
  using setter_traits = member_setter_traits<Setter>;
  static_assert(std::is_same_v<typename getter_traits::class_type, Class>,
                "Property getter owner must match builder type");
  static_assert(std::is_same_v<typename setter_traits::class_type, Class>,
                "Property setter owner must match builder type");
  static_assert(
      std::is_same_v<typename getter_traits::value_type, typename setter_traits::value_type>,
      "Property getter and setter value types must match");
  using value_type = typename getter_traits::value_type;
  if (!meta::resolve<value_type>()) {
    return {};
  }

  property_data property{
      string_name{name},
      0,
      true,
      property_storage_kind::accessor,
      accessor_property_ops<Class, Getter, Setter>(),
  };
  property.store_accessor_pointers(getter, setter);
  return property;
}

template <typename Class, typename Property>
[[nodiscard]] std::size_t member_offset(Property Class::* member) noexcept {
  const auto* object = reinterpret_cast<const volatile Class*>(0x1000);
  const auto* property = &(object->*member);
  return reinterpret_cast<std::uintptr_t>(property) - reinterpret_cast<std::uintptr_t>(object);
}

} // namespace caors::meta::detail

#endif
