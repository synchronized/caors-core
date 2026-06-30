// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_BUILDER_H_
#define CAORS_REFLECTION_BUILDER_H_

#include "caors/reflection/registry.h"

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

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
      : type_(make_meta_type_data<Class>(name, parent_id)) {}

  class_builder(std::string_view name, type_guid guid, type_id parent_id = type_id::invalid())
      : type_(make_meta_type_data<Class>(name, guid, parent_id)) {}

  template <typename Field> class_builder& field(std::string_view name, Field Class::* member) {
    using field_type = std::remove_cv_t<Field>;
    (void)detail::make_field_traits_info<field_type>();

    if (pending_status_) {
      return *this;
    }

    const std::optional<meta_type> resolved_type =
        get_reflection_registry().template find_registered_type<field_type>();
    if (resolved_type.has_value() && !resolved_type->name().empty()) {
      reflection_field_data field = make_reflection_field_data<Field>(
          name, resolved_type->name().view(), detail::member_offset(member));
      field.type = resolved_type->id();
      field.type_name = resolved_type->name();
      if (field.kind == field_kind::value || field.kind == field_kind::enumeration) {
        field.value_type = resolved_type->id();
      }
      enrich_sequence_field<field_type>(field);
      enrich_set_field<field_type>(field);
      enrich_associative_field<field_type>(field);
      reflection_.fields.push_back(field);
    } else {
      if constexpr (sequence_traits<field_type>::is_sequence) {
        const std::optional<string> sequence_name = sequence_type_name<field_type>();
        if (!sequence_name.has_value()) {
          pending_status_ = {error_domain::reflection, errc::invalid_field};
          return *this;
        }

        reflection_.fields.push_back(
            make_reflection_field_data<Field>(name, *sequence_name, detail::member_offset(member)));
        enrich_sequence_field<field_type>(reflection_.fields.back());
      } else if constexpr (set_traits<field_type>::is_set) {
        const std::optional<string> set_name = set_type_name<field_type>();
        if (!set_name.has_value()) {
          pending_status_ = {error_domain::reflection, errc::invalid_field};
          return *this;
        }

        reflection_.fields.push_back(
            make_reflection_field_data<Field>(name, *set_name, detail::member_offset(member)));
        enrich_set_field<field_type>(reflection_.fields.back());
      } else if constexpr (associative_traits<field_type>::is_associative) {
        const std::optional<string> associative_name = associative_type_name<field_type>();
        if (!associative_name.has_value()) {
          pending_status_ = {error_domain::reflection, errc::invalid_field};
          return *this;
        }

        reflection_.fields.push_back(make_reflection_field_data<Field>(
            name, *associative_name, detail::member_offset(member)));
        enrich_associative_field<field_type>(reflection_.fields.back());
      } else if constexpr (detail::has_type_name_v<field_type>) {
        reflection_.fields.push_back(make_reflection_field_data<Field>(
            name, type_name<field_type>::value, detail::member_offset(member)));
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

    reflection_field_data field =
        make_reflection_field_data<Field>(name, type_name, detail::member_offset(member));
    if (field.kind == field_kind::value || field.kind == field_kind::enumeration) {
      field.value_type = field.type;
    }
    enrich_sequence_field<std::remove_cv_t<Field>>(field);
    enrich_set_field<std::remove_cv_t<Field>>(field);
    enrich_associative_field<std::remove_cv_t<Field>>(field);
    reflection_.fields.push_back(field);
    return *this;
  }

  [[nodiscard]] error commit() {
    if (pending_status_) {
      return pending_status_;
    }

    return get_reflection_registry().template register_type_for<Class>(type_, reflection_);
  }

  [[nodiscard]] const meta_type_data& type_data() const noexcept { return type_; }
  [[nodiscard]] const reflection_type_data& reflection_data() const noexcept { return reflection_; }

private:
  template <typename Field> [[nodiscard]] std::optional<string> sequence_type_name() const {
    if constexpr (sequence_traits<Field>::is_sequence) {
      using element_type = typename sequence_traits<Field>::element_type;

      std::optional<meta_type> element =
          get_reflection_registry().template find_registered_type<element_type>();
      if (!element.has_value()) {
        if constexpr (detail::has_type_name_v<element_type>) {
          const error status = caors::register_type<element_type>(type_name<element_type>::value);
          if (status && status.value() != errc::duplicate_id) {
            return std::nullopt;
          }

          element = caors::find_type<element_type>();
        }
      }

      if (!element.has_value() || element->name().empty()) {
        return std::nullopt;
      }

      string name;
      name += sequence_traits<Field>::type_name_prefix();
      name += "<";
      name += element->name().view();
      name += ">";
      return name;
    } else {
      return std::nullopt;
    }
  }

  template <typename Field> void enrich_sequence_field(reflection_field_data& field) const {
    if constexpr (sequence_traits<Field>::is_sequence) {
      using element_type = typename sequence_traits<Field>::element_type;

      const std::optional<meta_type> element = caors::find_type<element_type>();
      if (element.has_value()) {
        field.element_type = element->id();
        field.element_type_name = element->name();
      }
    }
  }

  template <typename Field> [[nodiscard]] std::optional<string> set_type_name() const {
    if constexpr (set_traits<Field>::is_set) {
      using element_type = typename set_traits<Field>::element_type;

      const std::optional<meta_type> element = ensure_registered_type<element_type>();
      if (!element.has_value()) {
        return std::nullopt;
      }

      string name;
      name += set_traits<Field>::type_name_prefix();
      name += "<";
      name += element->name().view();
      name += ">";
      return name;
    } else {
      return std::nullopt;
    }
  }

  template <typename Field> void enrich_set_field(reflection_field_data& field) const {
    if constexpr (set_traits<Field>::is_set) {
      using element_type = typename set_traits<Field>::element_type;

      const std::optional<meta_type> element = caors::find_type<element_type>();
      if (element.has_value()) {
        field.element_type = element->id();
        field.element_type_name = element->name();
      }
    }
  }

  template <typename T> [[nodiscard]] std::optional<meta_type> ensure_registered_type() const {
    std::optional<meta_type> type = get_reflection_registry().template find_registered_type<T>();
    if (!type.has_value()) {
      if constexpr (detail::has_type_name_v<T>) {
        const error status = caors::register_type<T>(type_name<T>::value);
        if (status && status.value() != errc::duplicate_id) {
          return std::nullopt;
        }

        type = caors::find_type<T>();
      }
    }

    if (!type.has_value() || type->name().empty()) {
      return std::nullopt;
    }

    return type;
  }

  template <typename Field> [[nodiscard]] std::optional<string> associative_type_name() const {
    if constexpr (associative_traits<Field>::is_associative) {
      using key_type = typename associative_traits<Field>::key_type;
      using mapped_type = typename associative_traits<Field>::mapped_type;

      const std::optional<meta_type> key = ensure_registered_type<key_type>();
      const std::optional<meta_type> mapped = ensure_registered_type<mapped_type>();
      if (!key.has_value() || !mapped.has_value()) {
        return std::nullopt;
      }

      string name;
      name += associative_traits<Field>::type_name_prefix();
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

  template <typename Field> void enrich_associative_field(reflection_field_data& field) const {
    if constexpr (associative_traits<Field>::is_associative) {
      using key_type = typename associative_traits<Field>::key_type;
      using mapped_type = typename associative_traits<Field>::mapped_type;

      const std::optional<meta_type> key = caors::find_type<key_type>();
      if (key.has_value()) {
        field.key_type = key->id();
        field.key_type_name = key->name();
      }

      const std::optional<meta_type> mapped = caors::find_type<mapped_type>();
      if (mapped.has_value()) {
        field.mapped_type = mapped->id();
        field.mapped_type_name = mapped->name();
      }
    }
  }

  meta_type_data type_{};
  reflection_type_data reflection_{};
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

} // namespace caors::reflection

#endif
