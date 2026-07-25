// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_PROPERTY_MODEL_H_
#define CAORS_META_DETAIL_PROPERTY_MODEL_H_

#include "caors/meta/detail/property_record.h"

#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

template <typename T> [[nodiscard]] bool property_type_matches_name(std::string_view name) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  const meta::type registered = caors::meta::find<type>();
  return registered && !registered.name().empty() && registered.name() == name;
}

template <typename Value>
[[nodiscard]] bool assign_property_value(void* storage, const any* source) {
  using value_type = std::remove_cv_t<Value>;
  if constexpr (std::is_const_v<Value>) {
    return false;
  } else {
    const meta::type expected = meta::resolve<value_type>();
    const meta::value_storage_ops* value_ops = expected.get_raw_ops<meta::value_storage_ops>();
    if (storage == nullptr || source == nullptr || !source->valid() ||
        source->get_type().id() != expected.id() || value_ops == nullptr ||
        source->size() != value_ops->layout.size ||
        source->alignment() != value_ops->layout.alignment || value_ops->ops.assign == nullptr) {
      return false;
    }
    return value_ops->ops.assign(storage, source->data());
  }
}

template <typename Class, typename Property> struct member_property_model {
  std::size_t offset = 0;

  [[nodiscard]] const void* data(const void* object) const noexcept {
    return object != nullptr ? static_cast<const unsigned char*>(object) + offset : nullptr;
  }

  [[nodiscard]] void* mutable_data(void* object) const noexcept {
    if constexpr (std::is_const_v<Property>) {
      return nullptr;
    } else {
      return object != nullptr ? static_cast<unsigned char*>(object) + offset : nullptr;
    }
  }

  static bool invoke(void* state, property_request_kind kind, request_payload payload) {
    using value_type = std::remove_cv_t<Property>;
    auto& model = *static_cast<member_property_model*>(state);

    switch (kind) {
    case property_request_kind::owner_type:
      payload.get<property_type_request>().result = meta::resolve<Class>();
      return true;
    case property_request_kind::value_type:
      payload.get<property_type_request>().result = meta::resolve<value_type>();
      return true;
    case property_request_kind::storage_info:
      payload.get<property_storage_info>() = property_storage_info{
          property_storage_kind::member,
          model.offset,
          meta::resolve<value_type>(),
          !std::is_const_v<Property>,
      };
      return true;
    case property_request_kind::const_data: {
      auto& request = payload.get<property_const_data_request>();
      request.result = model.data(request.object);
      return true;
    }
    case property_request_kind::mutable_data: {
      auto& request = payload.get<property_mutable_data_request>();
      request.result = model.mutable_data(request.object);
      return true;
    }
    case property_request_kind::get_any: {
      auto& request = payload.get<property_get_any_request>();
      const auto* value = static_cast<const value_type*>(model.data(request.object));
      request.result =
          value != nullptr ? any::copy_from_raw(meta::resolve<value_type>(), value) : any{};
      return true;
    }
    case property_request_kind::get_const_ref: {
      auto& request = payload.get<property_get_const_ref_request>();
      const auto* value = static_cast<const value_type*>(model.data(request.object));
      request.result =
          value != nullptr ? any_ref::from(meta::resolve<value_type>(), value) : any_ref{};
      return true;
    }
    case property_request_kind::get_mutable_ref: {
      auto& request = payload.get<property_get_mutable_ref_request>();
      auto* value = static_cast<value_type*>(model.mutable_data(request.object));
      request.result =
          value != nullptr ? any_ref::from(meta::resolve<value_type>(), value) : any_ref{};
      return true;
    }
    case property_request_kind::set_any: {
      auto& request = payload.get<property_set_any_request>();
      request.result =
          assign_property_value<Property>(model.mutable_data(request.object), request.source);
      return true;
    }
    }

    return false;
  }
};

template <typename Getter> struct const_member_getter_traits;

template <typename Class, typename Return>
struct const_member_getter_traits<Return (Class::*)() const> {
  using class_type = Class;
  using return_type = Return;
  using value_type = std::remove_cv_t<std::remove_reference_t<Return>>;
};

template <typename Class, typename Return>
struct const_member_getter_traits<Return (Class::*)() const noexcept> {
  using class_type = Class;
  using return_type = Return;
  using value_type = std::remove_cv_t<std::remove_reference_t<Return>>;
};

template <typename Setter> struct member_setter_traits;

template <typename Class, typename Return, typename Argument>
struct member_setter_traits<Return (Class::*)(Argument)> {
  using class_type = Class;
  using return_type = Return;
  using argument_type = Argument;
  using value_type = std::remove_cv_t<std::remove_reference_t<Argument>>;
};

template <typename Class, typename Return, typename Argument>
struct member_setter_traits<Return (Class::*)(Argument) noexcept> {
  using class_type = Class;
  using return_type = Return;
  using argument_type = Argument;
  using value_type = std::remove_cv_t<std::remove_reference_t<Argument>>;
};

template <typename Class, typename Getter, typename Setter = std::nullptr_t>
struct accessor_property_model {
  using getter_traits = const_member_getter_traits<Getter>;
  using return_type = typename getter_traits::return_type;
  using value_type = typename getter_traits::value_type;

  Getter getter;
  Setter setter;

  [[nodiscard]] decltype(auto) get(const void* object) const {
    return std::invoke(getter, *static_cast<const Class*>(object));
  }

  [[nodiscard]] static constexpr bool writable() noexcept {
    return !std::is_same_v<Setter, std::nullptr_t>;
  }

  static bool invoke(void* state, property_request_kind kind, request_payload payload) {
    auto& model = *static_cast<accessor_property_model*>(state);

    switch (kind) {
    case property_request_kind::owner_type:
      payload.get<property_type_request>().result = meta::resolve<Class>();
      return true;
    case property_request_kind::value_type:
      payload.get<property_type_request>().result = meta::resolve<value_type>();
      return true;
    case property_request_kind::storage_info:
      payload.get<property_storage_info>() = property_storage_info{
          property_storage_kind::accessor,
          0,
          meta::resolve<value_type>(),
          writable(),
      };
      return true;
    case property_request_kind::const_data: {
      auto& request = payload.get<property_const_data_request>();
      if constexpr (std::is_lvalue_reference_v<return_type>) {
        request.result =
            request.object != nullptr ? std::addressof(model.get(request.object)) : nullptr;
      } else {
        request.result = nullptr;
      }
      return true;
    }
    case property_request_kind::mutable_data: {
      auto& request = payload.get<property_mutable_data_request>();
      if constexpr (std::is_lvalue_reference_v<return_type> &&
                    !std::is_const_v<std::remove_reference_t<return_type>>) {
        request.result =
            request.object != nullptr ? std::addressof(model.get(request.object)) : nullptr;
      } else {
        request.result = nullptr;
      }
      return true;
    }
    case property_request_kind::get_any: {
      auto& request = payload.get<property_get_any_request>();
      request.result = request.object != nullptr ? any::from(model.get(request.object)) : any{};
      return true;
    }
    case property_request_kind::get_const_ref: {
      auto& request = payload.get<property_get_const_ref_request>();
      if constexpr (std::is_lvalue_reference_v<return_type>) {
        request.result = request.object != nullptr
                             ? any_ref::from(meta::resolve<value_type>(),
                                             std::addressof(model.get(request.object)))
                             : any_ref{};
      } else {
        request.result = {};
      }
      return true;
    }
    case property_request_kind::get_mutable_ref: {
      auto& request = payload.get<property_get_mutable_ref_request>();
      if constexpr (std::is_lvalue_reference_v<return_type> &&
                    !std::is_const_v<std::remove_reference_t<return_type>>) {
        request.result = request.object != nullptr
                             ? any_ref::from(meta::resolve<value_type>(),
                                             std::addressof(model.get(request.object)))
                             : any_ref{};
      } else {
        request.result = {};
      }
      return true;
    }
    case property_request_kind::set_any: {
      auto& request = payload.get<property_set_any_request>();
      request.result = false;
      if constexpr (!std::is_same_v<Setter, std::nullptr_t>) {
        using setter_traits = member_setter_traits<Setter>;
        using setter_return = typename setter_traits::return_type;
        using argument_type = typename setter_traits::argument_type;
        static_assert(!std::is_rvalue_reference_v<argument_type>);
        static_assert(!std::is_lvalue_reference_v<argument_type> ||
                      std::is_const_v<std::remove_reference_t<argument_type>>);
        static_assert(std::is_void_v<setter_return> || std::is_same_v<setter_return, bool>);

        const value_type* value =
            request.source != nullptr ? request.source->template try_get<value_type>() : nullptr;
        if (request.object != nullptr && value != nullptr) {
          auto& object = *static_cast<Class*>(request.object);
          if constexpr (std::is_void_v<setter_return>) {
            std::invoke(model.setter, object, *value);
            request.result = true;
          } else {
            request.result = std::invoke(model.setter, object, *value);
          }
        }
      }
      return true;
    }
    }

    return false;
  }
};

template <typename Property> struct global_property_model {
  Property* value = nullptr;
  using value_type = std::remove_cv_t<Property>;

  static bool invoke(void* state, property_request_kind kind, request_payload payload) {
    auto& model = *static_cast<global_property_model*>(state);

    switch (kind) {
    case property_request_kind::owner_type:
      payload.get<property_type_request>().result = {};
      return true;
    case property_request_kind::value_type:
      payload.get<property_type_request>().result = meta::resolve<value_type>();
      return true;
    case property_request_kind::storage_info:
      payload.get<property_storage_info>() = property_storage_info{
          property_storage_kind::global,
          0,
          meta::resolve<value_type>(),
          !std::is_const_v<Property>,
      };
      return true;
    case property_request_kind::const_data:
      payload.get<property_const_data_request>().result = model.value;
      return true;
    case property_request_kind::mutable_data:
      if constexpr (std::is_const_v<Property>) {
        payload.get<property_mutable_data_request>().result = nullptr;
      } else {
        payload.get<property_mutable_data_request>().result = model.value;
      }
      return true;
    case property_request_kind::get_any: {
      auto& request = payload.get<property_get_any_request>();
      request.result = model.value != nullptr
                           ? any::copy_from_raw(meta::resolve<value_type>(), model.value)
                           : any{};
      return true;
    }
    case property_request_kind::get_const_ref: {
      auto& request = payload.get<property_get_const_ref_request>();
      request.result = model.value != nullptr
                           ? any_ref::from(meta::resolve<value_type>(), model.value)
                           : any_ref{};
      return true;
    }
    case property_request_kind::get_mutable_ref: {
      auto& request = payload.get<property_get_mutable_ref_request>();
      if constexpr (std::is_const_v<Property>) {
        request.result = {};
      } else {
        request.result = model.value != nullptr
                             ? any_ref::from(meta::resolve<value_type>(), model.value)
                             : any_ref{};
      }
      return true;
    }
    case property_request_kind::set_any: {
      auto& request = payload.get<property_set_any_request>();
      if constexpr (std::is_const_v<Property>) {
        request.result = false;
      } else {
        request.result = assign_property_value<Property>(model.value, request.source);
      }
      return true;
    }
    }

    return false;
  }
};

template <typename Class, typename Property>
[[nodiscard]] property_record make_property_record(std::string_view name,
                                                   std::string_view type_name, std::size_t offset) {
  using property_type = std::remove_cv_t<Property>;
  if (!meta::resolve<property_type>() ||
      (!type_name.empty() && !property_type_matches_name<property_type>(type_name))) {
    return {};
  }

  using model_type = member_property_model<Class, Property>;
  return property_record{string_name{name}, make_property_wrapper<model_type>(model_type{offset})};
}

template <typename Property>
[[nodiscard]] property_record make_global_property_record(std::string_view name,
                                                          Property* pointer) {
  using property_type = std::remove_cv_t<Property>;
  if (pointer == nullptr || !meta::resolve<property_type>()) {
    return {};
  }

  using model_type = global_property_model<Property>;
  return property_record{string_name{name}, make_property_wrapper<model_type>(model_type{pointer})};
}

template <typename Class, typename Getter>
[[nodiscard]] property_record make_readonly_property_record(std::string_view name, Getter getter) {
  using getter_traits = const_member_getter_traits<Getter>;
  static_assert(std::is_same_v<typename getter_traits::class_type, Class>);
  using value_type = typename getter_traits::value_type;
  if (!meta::resolve<value_type>()) {
    return {};
  }

  using model_type = accessor_property_model<Class, Getter>;
  return property_record{
      string_name{name},
      make_property_wrapper<model_type>(model_type{getter, nullptr}),
  };
}

template <typename Class, typename Getter, typename Setter>
[[nodiscard]] property_record make_accessor_property_record(std::string_view name, Getter getter,
                                                            Setter setter) {
  using getter_traits = const_member_getter_traits<Getter>;
  using setter_traits = member_setter_traits<Setter>;
  static_assert(std::is_same_v<typename getter_traits::class_type, Class>);
  static_assert(std::is_same_v<typename setter_traits::class_type, Class>);
  static_assert(
      std::is_same_v<typename getter_traits::value_type, typename setter_traits::value_type>);
  using value_type = typename getter_traits::value_type;
  if (!meta::resolve<value_type>()) {
    return {};
  }

  using model_type = accessor_property_model<Class, Getter, Setter>;
  return property_record{
      string_name{name},
      make_property_wrapper<model_type>(model_type{getter, setter}),
  };
}

template <typename Class, typename Property>
[[nodiscard]] std::size_t member_offset(Property Class::* member) noexcept {
  const auto* object = reinterpret_cast<const volatile Class*>(0x1000);
  const auto* property = &(object->*member);
  return reinterpret_cast<std::uintptr_t>(property) - reinterpret_cast<std::uintptr_t>(object);
}

} // namespace caors::meta::detail

#endif
