// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_BUILDER_H_
#define CAORS_REFLECTION_BUILDER_H_

#include "caors/reflection/registry.h"

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::reflection {

/**
 * @brief RTTR 风格的反射注册构建器。
 *
 * 构建器通过成员指针推导字段大小、对齐和偏移；字段类型会优先使用同一注册表中已注册的 C++
 * 类型映射，未注册时再使用 `type_name<T>` 默认映射。
 */
template <typename Class> class class_builder {
public:
  explicit class_builder(std::string_view name, type_id parent_id = type_id::invalid())
      : type_(meta::make_type_data<Class>(name, parent_id)) {}

  class_builder(std::string_view name, type_guid guid, type_id parent_id = type_id::invalid())
      : type_(meta::make_type_data<Class>(name, guid, parent_id)) {}

  explicit class_builder(meta::type_view type) : existing_type_(type) {
    if (!type.valid()) {
      pending_status_ = {error_domain::reflection, errc::invalid_type};
    }
  }

  template <typename Field> class_builder& field(std::string_view name, Field Class::* member) {
    using field_type = std::remove_cv_t<Field>;

    if (pending_status_) {
      return *this;
    }

    const std::optional<meta::type_view> resolved_type =
        get_registry().template find_registered_type<field_type>();
    if (resolved_type.has_value() && !resolved_type->name().empty()) {
      field_data field =
          make_field_data<Field>(name, resolved_type->name().view(), detail::member_offset(member));
      field.type = resolved_type->id();
      reflection_.fields.push_back(field);
    } else {
      if constexpr (adapters::sequence_traits<field_type>::is_sequence) {
        const std::optional<string> sequence_name = sequence_type_name<field_type>();
        if (!sequence_name.has_value()) {
          pending_status_ = {error_domain::reflection, errc::invalid_field};
          return *this;
        }

        reflection_.fields.push_back(
            make_field_data<Field>(name, *sequence_name, detail::member_offset(member)));
      } else if constexpr (adapters::set_traits<field_type>::is_set) {
        const std::optional<string> set_name = set_type_name<field_type>();
        if (!set_name.has_value()) {
          pending_status_ = {error_domain::reflection, errc::invalid_field};
          return *this;
        }

        reflection_.fields.push_back(
            make_field_data<Field>(name, *set_name, detail::member_offset(member)));
      } else if constexpr (adapters::associative_traits<field_type>::is_associative) {
        const std::optional<string> associative_name = associative_type_name<field_type>();
        if (!associative_name.has_value()) {
          pending_status_ = {error_domain::reflection, errc::invalid_field};
          return *this;
        }

        reflection_.fields.push_back(
            make_field_data<Field>(name, *associative_name, detail::member_offset(member)));
      } else if constexpr (adapters::pair_traits<field_type>::is_pair) {
        const std::optional<string> pair_name = pair_type_name<field_type>();
        if (!pair_name.has_value()) {
          pending_status_ = {error_domain::reflection, errc::invalid_field};
          return *this;
        }

        reflection_.fields.push_back(
            make_field_data<Field>(name, *pair_name, detail::member_offset(member)));
      } else if constexpr (adapters::tuple_traits<field_type>::is_tuple) {
        const std::optional<string> tuple_name = tuple_type_name<field_type>();
        if (!tuple_name.has_value()) {
          pending_status_ = {error_domain::reflection, errc::invalid_field};
          return *this;
        }

        reflection_.fields.push_back(
            make_field_data<Field>(name, *tuple_name, detail::member_offset(member)));
      } else if constexpr (adapters::variant_traits<field_type>::is_variant) {
        const std::optional<string> variant_name = variant_type_name<field_type>();
        if (!variant_name.has_value()) {
          pending_status_ = {error_domain::reflection, errc::invalid_field};
          return *this;
        }

        reflection_.fields.push_back(
            make_field_data<Field>(name, *variant_name, detail::member_offset(member)));
      } else if constexpr (detail::has_type_name_v<field_type>) {
        reflection_.fields.push_back(make_field_data<Field>(name, type_name<field_type>::value,
                                                            detail::member_offset(member)));
      } else {
        pending_status_ = {error_domain::reflection, errc::invalid_field};
      }
    }

    return *this;
  }

  template <typename Field>
  class_builder& field(std::string_view name, Field Class::* member, std::string_view type_name) {
    if (pending_status_) {
      return *this;
    }

    field_data field = make_field_data<Field>(name, type_name, detail::member_offset(member));
    reflection_.fields.push_back(field);
    return *this;
  }

  [[nodiscard]] error commit() {
    if (pending_status_) {
      return pending_status_;
    }

    if (existing_type_.valid()) {
      return get_registry().register_type(existing_type_, reflection_);
    }

    return get_registry().template register_type_for<Class>(type_, reflection_);
  }

  [[nodiscard]] const meta::type_data& type_data() const noexcept { return type_; }
  [[nodiscard]] const reflection::type_data& reflection_data() const noexcept {
    return reflection_;
  }

private:
  template <typename Field> [[nodiscard]] std::optional<string> sequence_type_name() const {
    if constexpr (adapters::sequence_traits<Field>::is_sequence) {
      using element_type = typename adapters::sequence_traits<Field>::element_type;

      std::optional<meta::type_view> element =
          get_registry().template find_registered_type<element_type>();
      if (!element.has_value()) {
        if constexpr (detail::has_type_name_v<element_type>) {
          const error status =
              caors::meta::detail::register_type<element_type>(type_name<element_type>::value);
          if (status && status.value() != errc::duplicate_id) {
            return std::nullopt;
          }

          element = caors::meta::find<element_type>();
        }
      }

      if (!element.has_value() || element->name().empty()) {
        return std::nullopt;
      }

      string name;
      name += adapters::sequence_traits<Field>::type_name_prefix();
      name += "<";
      name += element->name().view();
      if constexpr (requires { adapters::sequence_traits<Field>::fixed_size; }) {
        name += ",";
        const std::string fixed_size = std::to_string(adapters::sequence_traits<Field>::fixed_size);
        name.append(fixed_size.data(), fixed_size.size());
      }
      name += ">";
      return name;
    } else {
      return std::nullopt;
    }
  }

  template <typename Field> [[nodiscard]] std::optional<string> set_type_name() const {
    if constexpr (adapters::set_traits<Field>::is_set) {
      using element_type = typename adapters::set_traits<Field>::element_type;

      const std::optional<meta::type_view> element = ensure_registered_type<element_type>();
      if (!element.has_value()) {
        return std::nullopt;
      }

      string name;
      name += adapters::set_traits<Field>::type_name_prefix();
      name += "<";
      name += element->name().view();
      name += ">";
      return name;
    } else {
      return std::nullopt;
    }
  }

  template <typename T>
  [[nodiscard]] std::optional<meta::type_view> ensure_registered_type() const {
    std::optional<meta::type_view> type = get_registry().template find_registered_type<T>();
    if (!type.has_value()) {
      if constexpr (detail::has_type_name_v<T>) {
        const error status = caors::meta::detail::register_type<T>(type_name<T>::value);
        if (status && status.value() != errc::duplicate_id) {
          return std::nullopt;
        }

        type = caors::meta::find<T>();
      }
    }

    if (!type.has_value() || type->name().empty()) {
      return std::nullopt;
    }

    return type;
  }

  template <typename Field> [[nodiscard]] std::optional<string> associative_type_name() const {
    if constexpr (adapters::associative_traits<Field>::is_associative) {
      using key_type = typename adapters::associative_traits<Field>::key_type;
      using mapped_type = typename adapters::associative_traits<Field>::mapped_type;

      const std::optional<meta::type_view> key = ensure_registered_type<key_type>();
      const std::optional<meta::type_view> mapped = ensure_registered_type<mapped_type>();
      if (!key.has_value() || !mapped.has_value()) {
        return std::nullopt;
      }

      string name;
      name += adapters::associative_traits<Field>::type_name_prefix();
      name += "<";
      name += key->name().view();
      name += ",";
      name += mapped->name().view();
      name += ">";
      return name;
    } else {
      return std::nullopt;
    }
  }

  template <typename Element>
  [[nodiscard]] bool append_registered_type_name(string& name, bool& first) const {
    const std::optional<meta::type_view> type = ensure_registered_type<Element>();
    if (!type.has_value()) {
      return false;
    }

    if (!first) {
      name += ",";
    }

    name += type->name().view();
    first = false;
    return true;
  }

  template <typename Field, std::size_t... Indices>
  [[nodiscard]] std::optional<string> tuple_type_name_impl(std::index_sequence<Indices...>) const {
    string name;
    name += adapters::tuple_traits<Field>::type_name_prefix();
    name += "<";

    bool first = true;
    const bool succeeded =
        (append_registered_type_name<
             typename adapters::tuple_traits<Field>::template element_type<Indices>>(name, first) &&
         ... && true);
    if (!succeeded) {
      return std::nullopt;
    }

    name += ">";
    return name;
  }

  template <typename Field> [[nodiscard]] std::optional<string> tuple_type_name() const {
    if constexpr (adapters::tuple_traits<Field>::is_tuple) {
      return tuple_type_name_impl<Field>(
          std::make_index_sequence<adapters::tuple_traits<Field>::size>{});
    } else {
      return std::nullopt;
    }
  }

  template <typename Field, std::size_t... Indices>
  [[nodiscard]] std::optional<string>
  variant_type_name_impl(std::index_sequence<Indices...>) const {
    string name;
    name += adapters::variant_traits<Field>::type_name_prefix();
    name += "<";

    bool first = true;
    const bool succeeded =
        (append_registered_type_name<
             typename adapters::variant_traits<Field>::template alternative_type<Indices>>(name,
                                                                                           first) &&
         ... && true);
    if (!succeeded) {
      return std::nullopt;
    }

    name += ">";
    return name;
  }

  template <typename Field> [[nodiscard]] std::optional<string> variant_type_name() const {
    if constexpr (adapters::variant_traits<Field>::is_variant) {
      return variant_type_name_impl<Field>(
          std::make_index_sequence<adapters::variant_traits<Field>::size>{});
    } else {
      return std::nullopt;
    }
  }

  template <typename Field> [[nodiscard]] std::optional<string> pair_type_name() const {
    if constexpr (adapters::pair_traits<Field>::is_pair) {
      using first_type = typename adapters::pair_traits<Field>::first_type;
      using second_type = typename adapters::pair_traits<Field>::second_type;

      const std::optional<meta::type_view> first = ensure_registered_type<first_type>();
      const std::optional<meta::type_view> second = ensure_registered_type<second_type>();
      if (!first.has_value() || !second.has_value()) {
        return std::nullopt;
      }

      string name;
      name += adapters::pair_traits<Field>::type_name_prefix();
      name += "<";
      name += first->name().view();
      name += ",";
      name += second->name().view();
      name += ">";
      return name;
    } else {
      return std::nullopt;
    }
  }

  meta::type_data type_{};
  meta::type_view existing_type_{};
  reflection::type_data reflection_{};
  error pending_status_{};
};

/** @brief 创建反射注册构建器。 */
template <typename Class>
[[nodiscard]] class_builder<Class> class_(std::string_view name,
                                          type_id parent_id = type_id::invalid()) {
  return class_builder<Class>{name, parent_id};
}

/** @brief 创建带稳定 GUID 的反射注册构建器。 */
template <typename Class>
[[nodiscard]] class_builder<Class> class_(std::string_view name, type_guid guid,
                                          type_id parent_id = type_id::invalid()) {
  return class_builder<Class>{name, guid, parent_id};
}

/** @brief 为已注册元类型创建反射注册构建器，不会创建或修改基础类型记录。 */
template <typename Class> [[nodiscard]] class_builder<Class> class_(meta::type_view type) {
  return class_builder<Class>{type};
}

} // namespace caors::reflection

#endif
