// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_OPERATIONS_H_
#define CAORS_SERIALIZATION_OPERATIONS_H_

#include "caors/io/file_system.h"
#include "caors/meta/object.h"
#include "caors/meta/property.h"
#include "caors/meta/query.h"
#include "caors/meta/type.h"
#include "caors/object.h"
#include "caors/serialization/codec.h"
#include "caors/serialization/detail/operations.h"
#include "caors/serialization/dom_reader.h"
#include "caors/serialization/json_reader.h"
#include "caors/serialization/json_writer.h"
#include "caors/serialization/messagepack.h"
#include "caors/serialization/schema_query.h"
#include "caors/serialization/writer.h"

#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::serialization {

namespace detail {

template <typename T>
[[nodiscard]] meta::type find_registered_type(const serialization::context& context) {
  return caors::meta::find<T>(context.effective_type_context());
}

[[nodiscard]] inline meta::type find_registered_type(const serialization::context& context,
                                                     type_id id) {
  return meta::find(context.effective_type_context(), id);
}

} // namespace detail

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象写入指定后端。 */
template <typename T> [[nodiscard]] error serialize(const T& object, writer& writer) {
  serialization::context context{};
  const meta::type type = detail::find_registered_type<T>(context);
  if (!type.registered()) {
    return {error_domain::serialization, errc::missing_type};
  }

  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_base_of_v<meta::object, value_type>) {
    if (detail::object_requires_polymorphic_envelope(type.id(), object)) {
      return detail::serialize_polymorphic_object(context, type.id(), object, writer);
    }
  }

  const meta::type meta_type = detail::find_registered_type(context, type.id());
  if (!meta_type) {
    return {error_domain::serialization, errc::not_found};
  }

  return detail::serialize(context, meta_type, &object, writer);
}

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象按上下文写入指定后端。 */
template <typename T>
[[nodiscard]] error serialize(const serialization::context& context, const T& object,
                              writer& writer) {
  const meta::type type = detail::find_registered_type<T>(context);
  if (!type.registered()) {
    return {error_domain::serialization, errc::missing_type};
  }

  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_base_of_v<meta::object, value_type>) {
    if (detail::object_requires_polymorphic_envelope(type.id(), object)) {
      return detail::serialize_polymorphic_object(context, type.id(), object, writer);
    }
  }

  const meta::type meta_type = detail::find_registered_type(context, type.id());
  if (!meta_type) {
    return {error_domain::serialization, errc::not_found};
  }

  return detail::serialize(context, meta_type, &object, writer);
}

/** @brief 将序列化 DOM 中间值写入指定后端。 */
[[nodiscard]] CAORS_API error write_value_to(writer& writer, const variant& value);

/** @brief 解析 JSON 文本为序列化 DOM 中间值。 */
[[nodiscard]] CAORS_API variant_result parse_json(std::string_view text);

/** @brief 按格式选项解析 JSON 文本为序列化 DOM 中间值。 */
[[nodiscard]] CAORS_API variant_result parse_json(std::string_view text, json_options options);

/** @brief 从输入流读取 JSON 并解析为序列化 DOM 中间值。 */
[[nodiscard]] CAORS_API variant_result parse_json(io::input_stream& stream,
                                                  memory_resource* resource = nullptr);

/** @brief 从输入流读取 JSON 并按格式选项解析为序列化 DOM 中间值。 */
[[nodiscard]] CAORS_API variant_result parse_json(io::input_stream& stream, json_options options,
                                                  memory_resource* resource = nullptr);

/** @brief 从文件系统路径读取 JSON 并解析为序列化 DOM 中间值。 */
[[nodiscard]] CAORS_API variant_result parse_json_from(io::file_system& file_system,
                                                       io::path_view path,
                                                       memory_resource* resource = nullptr);

/** @brief 从文件系统路径读取 JSON 并按格式选项解析为序列化 DOM 中间值。 */
[[nodiscard]] CAORS_API variant_result parse_json_from(io::file_system& file_system,
                                                       io::path_view path, json_options options,
                                                       memory_resource* resource = nullptr);

namespace detail {

[[nodiscard]] inline core::string_name
schema_serialized_field_name(const serialization::field_schema& field) noexcept {
  return field.serialized_name.empty() ? field.field_name : field.serialized_name;
}

[[nodiscard]] inline bool
schema_member_matches_field(const meta::variant_member& member,
                            const serialization::field_schema& field) noexcept {
  const core::string_name name = schema_serialized_field_name(field);
  if (field.field_id != 0 && member.field_id == field.field_id) {
    return true;
  }

  if (field.legacy_field_id != 0 && member.field_id == field.legacy_field_id) {
    return true;
  }

  if (!name.empty() && member.name == name) {
    return true;
  }

  return !field.legacy_serialized_name.empty() && member.name == field.legacy_serialized_name;
}

[[nodiscard]] inline bool schema_member_is_known(const serialization::type_schema& schema,
                                                 const meta::variant_member& member) noexcept {
  for (const serialization::field_schema& field : schema.fields) {
    if (schema_member_matches_field(member, field)) {
      return true;
    }
  }

  return false;
}

[[nodiscard]] inline const serialization::field_schema*
find_schema_member_field(const serialization::type_schema& schema,
                         const meta::variant_member& member) noexcept {
  for (const serialization::field_schema& field : schema.fields) {
    if (schema_member_matches_field(member, field)) {
      return &field;
    }
  }

  return nullptr;
}

[[nodiscard]] inline schema_path append_schema_member_path(const schema_path& path,
                                                           const meta::variant_member& member) {
  schema_path nested = path;
  nested.push_back(schema_path_segment{
      schema_path_segment_kind::member,
      member.name,
      member.field_id,
      0,
  });
  return nested;
}

[[nodiscard]] inline schema_path append_schema_index_path(const schema_path& path,
                                                          schema_path_segment_kind kind,
                                                          std::size_t index) {
  schema_path nested = path;
  nested.push_back(schema_path_segment{
      kind,
      {},
      0,
      index,
  });
  return nested;
}

[[nodiscard]] inline meta::type schema_unwrap_inline_indirect_type(meta::type type) {
  for (std::uint32_t depth = 0; type && depth < 64; ++depth) {
    const meta::type_kind kind = type.kind();
    if ((kind != meta::type_kind::raw_pointer && kind != meta::type_kind::wrapper) ||
        type.wrapper() != meta::wrapper_semantics::inline_value) {
      break;
    }

    type = type.get_value_type();
  }

  return type;
}

[[nodiscard]] inline meta::type schema_field_type(const meta::type& owner_type,
                                                  const serialization::field_schema& field) {
  const meta::property property =
      owner_type.find_property(field.field_name, meta::member_search::include_bases);
  return property ? property.get_type() : meta::type{};
}

[[nodiscard]] inline error schema_check_path_depth(const serialization::context& context,
                                                   const schema_path& path) noexcept {
  return context.config.limits.max_depth != 0 && path.size() >= context.config.limits.max_depth
             ? error{error_domain::serialization, errc::recursion_limit}
             : error{};
}

[[nodiscard]] inline error apply_schema_migration_tree(const serialization::context& context,
                                                       const meta::type& type, variant& value,
                                                       const schema_path& path);

[[nodiscard]] inline error apply_unknown_field_policy_tree(const serialization::context& context,
                                                           const meta::type& type,
                                                           const variant& value,
                                                           const schema_path& path);

[[nodiscard]] inline error
apply_schema_migration_to_nested_value(const serialization::context& context, meta::type value_type,
                                       variant& value, const schema_path& path) {
  if (const error status = schema_check_path_depth(context, path); status) {
    return status;
  }

  value_type = schema_unwrap_inline_indirect_type(value_type);
  if (value_type.kind() == meta::type_kind::sequence || value_type.kind() == meta::type_kind::set) {
    const meta::type element_type = schema_unwrap_inline_indirect_type(value_type.get_value_type());
    variant::array_type* elements = value.try_mutable_array();
    if (elements == nullptr || !serialization::contains(element_type)) {
      return {};
    }

    for (std::size_t index = 0; index < elements->size(); ++index) {
      schema_path element_path =
          append_schema_index_path(path, schema_path_segment_kind::element, index);
      if (const error status =
              apply_schema_migration_tree(context, element_type, (*elements)[index], element_path);
          status) {
        return status;
      }
    }

    return {};
  }

  if (value_type.kind() == meta::type_kind::associative) {
    const meta::type mapped_type = schema_unwrap_inline_indirect_type(value_type.get_value_type());
    variant::dictionary_type* entries = value.try_mutable_dictionary();
    if (entries == nullptr || !serialization::contains(mapped_type)) {
      return {};
    }

    for (std::size_t index = 0; index < entries->size(); ++index) {
      schema_path mapped_path =
          append_schema_index_path(path, schema_path_segment_kind::dictionary_value, index);
      if (const error status = apply_schema_migration_tree(context, mapped_type,
                                                           (*entries)[index].value, mapped_path);
          status) {
        return status;
      }
    }

    return {};
  }

  if (!value.is_object() || !serialization::contains(value_type)) {
    return {};
  }

  return apply_schema_migration_tree(context, value_type, value, path);
}

[[nodiscard]] inline error
apply_unknown_field_policy_to_nested_value(const serialization::context& context,
                                           meta::type value_type, const variant& value,
                                           const schema_path& path) {
  if (const error status = schema_check_path_depth(context, path); status) {
    return status;
  }

  value_type = schema_unwrap_inline_indirect_type(value_type);
  if (value_type.kind() == meta::type_kind::sequence || value_type.kind() == meta::type_kind::set) {
    const meta::type element_type = schema_unwrap_inline_indirect_type(value_type.get_value_type());
    const variant::array_type* elements = value.try_as_array();
    if (elements == nullptr || !serialization::contains(element_type)) {
      return {};
    }

    for (std::size_t index = 0; index < elements->size(); ++index) {
      schema_path element_path =
          append_schema_index_path(path, schema_path_segment_kind::element, index);
      if (const error status = apply_unknown_field_policy_tree(context, element_type,
                                                               (*elements)[index], element_path);
          status) {
        return status;
      }
    }

    return {};
  }

  if (value_type.kind() == meta::type_kind::associative) {
    const meta::type mapped_type = schema_unwrap_inline_indirect_type(value_type.get_value_type());
    const variant::dictionary_type* entries = value.try_as_dictionary();
    if (entries == nullptr || !serialization::contains(mapped_type)) {
      return {};
    }

    for (std::size_t index = 0; index < entries->size(); ++index) {
      schema_path mapped_path =
          append_schema_index_path(path, schema_path_segment_kind::dictionary_value, index);
      if (const error status = apply_unknown_field_policy_tree(
              context, mapped_type, (*entries)[index].value, mapped_path);
          status) {
        return status;
      }
    }

    return {};
  }

  if (!value.is_object() || !serialization::contains(value_type)) {
    return {};
  }

  return apply_unknown_field_policy_tree(context, value_type, value, path);
}

[[nodiscard]] inline error apply_unknown_field_policy(const serialization::context& context,
                                                      const meta::type& type,
                                                      const variant& value) {
  return apply_unknown_field_policy_tree(context, type, value, schema_path{});
}

[[nodiscard]] inline error apply_unknown_field_policy_tree(const serialization::context& context,
                                                           const meta::type& type,
                                                           const variant& value,
                                                           const schema_path& path) {
  if (context.config.schema.unknown_fields == unknown_field_policy::ignore || !value.is_object()) {
    return {};
  }

  const serialization::schema_view schema_view =
      type.get_capability<serialization::schema_capability>();
  const serialization::type_schema* schema = schema_view.raw();
  if (schema == nullptr) {
    return {error_domain::serialization, errc::missing_type};
  }

  if (!schema_view.valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  for (const meta::variant_member& member : value.as_object()) {
    const serialization::field_schema* field = find_schema_member_field(*schema, member);
    if (field != nullptr) {
      const meta::type field_type = schema_field_type(type, *field);
      schema_path nested_path = append_schema_member_path(path, member);
      if (const error status = apply_unknown_field_policy_to_nested_value(
              context, field_type, member.value, nested_path);
          status) {
        return status;
      }
      continue;
    }

    switch (context.config.schema.unknown_fields) {
    case unknown_field_policy::ignore:
      break;
    case unknown_field_policy::preserve:
      if (context.state.schema.preserved_unknown_fields != nullptr) {
        context.state.schema.preserved_unknown_fields->push_back(member);
      }
      if (context.state.schema.preserved_unknown_field_entries != nullptr) {
        context.state.schema.preserved_unknown_field_entries->push_back(preserved_unknown_field{
            path,
            type,
            member,
        });
      }
      break;
    case unknown_field_policy::reject:
      return {error_domain::serialization, errc::invalid_field};
    }
  }

  return {};
}

[[nodiscard]] inline error apply_schema_migration(const serialization::context& context,
                                                  const meta::type& type, variant& value) {
  return apply_schema_migration_tree(context, type, value, schema_path{});
}

[[nodiscard]] inline error apply_schema_migration_tree(const serialization::context& context,
                                                       const meta::type& type, variant& value,
                                                       const schema_path& path) {
  const serialization::schema_migration_ops* migration = context.effective_schema_migration();
  if (migration == nullptr || !migration->valid() || !value.is_object()) {
    return {};
  }

  const serialization::schema_migration_request request{
      type,
      &path,
      context.effective_schema_source_version(),
      context.effective_schema_version(),
  };
  if (const error status = (*migration)(request, value); status) {
    return status;
  }

  const serialization::schema_view schema_view =
      type.get_capability<serialization::schema_capability>();
  const serialization::type_schema* schema = schema_view.raw();
  if (schema == nullptr || !schema_view.valid()) {
    return {};
  }

  variant::object_type* members = value.try_mutable_object();
  if (members == nullptr) {
    return {};
  }

  for (meta::variant_member& member : *members) {
    const serialization::field_schema* field = find_schema_member_field(*schema, member);
    if (field == nullptr) {
      continue;
    }

    const meta::type field_type = schema_field_type(type, *field);
    schema_path nested_path = append_schema_member_path(path, member);
    if (const error status =
            apply_schema_migration_to_nested_value(context, field_type, member.value, nested_path);
        status) {
      return status;
    }
  }

  return {};
}

template <typename T>
[[nodiscard]] error read_migrated_dom_value_from(const serialization::context& context, T& object,
                                                 const variant& value) {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_base_of_v<meta::object, value_type>) {
    if (detail::is_polymorphic_value(context, value)) {
      const meta::type type = find_registered_type<value_type>(context);
      if (!type.registered()) {
        return {error_domain::serialization, errc::missing_type};
      }

      return detail::deserialize_polymorphic_object_into(context, type.id(), object, value);
    }
  }

  const meta::type type = find_registered_type<value_type>(context);
  if (!type.registered()) {
    return {error_domain::serialization, errc::missing_type};
  }

  const meta::type meta_type = find_registered_type(context, type.id());
  if (!meta_type) {
    return {error_domain::serialization, errc::not_found};
  }

  if (context.config.schema.unknown_fields != unknown_field_policy::ignore && value.is_object()) {
    if (const error status = apply_unknown_field_policy(context, meta_type, value); status) {
      return status;
    }
  }

  serialization::variant_reader reader{value};
  return detail::deserialize(context, meta_type, &object, reader);
}

template <typename T>
[[nodiscard]] error read_dom_value_from(const serialization::context& context, T& object,
                                        const variant& value) {
  const serialization::schema_migration_ops* migration = context.effective_schema_migration();
  if (migration == nullptr || !migration->valid()) {
    return read_migrated_dom_value_from(context, object, value);
  }

  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  const meta::type type = find_registered_type<value_type>(context);
  if (!type.registered()) {
    return {error_domain::serialization, errc::missing_type};
  }

  const meta::type meta_type = find_registered_type(context, type.id());
  if (!meta_type) {
    return {error_domain::serialization, errc::not_found};
  }

  variant migrated = value;
  if (const error status = apply_schema_migration(context, meta_type, migrated); status) {
    return status;
  }

  return read_migrated_dom_value_from(context, object, migrated);
}

template <typename T>
[[nodiscard]] error read_json_text_from(const serialization::context& context, T& object,
                                        std::string_view text, json_options options) {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;

  const serialization::schema_migration_ops* migration = context.effective_schema_migration();
  if (context.config.schema.unknown_fields != unknown_field_policy::ignore ||
      context.config.schema.missing_fields != missing_field_policy::reject ||
      (migration != nullptr && migration->valid())) {
    variant_result value = parse_json(text, options);
    if (value.failed()) {
      return value.status;
    }

    return read_dom_value_from(context, object, value.value);
  }

  const meta::type type = find_registered_type<value_type>(context);
  if (!type.registered()) {
    return {error_domain::serialization, errc::missing_type};
  }

  serialization::json_reader reader{text, options};
  if constexpr (std::is_base_of_v<meta::object, value_type>) {
    if (reader.is_polymorphic_envelope(context)) {
      return deserialize_polymorphic_object_into(context, type.id(), object, reader);
    }
  }

  const meta::type meta_type = find_registered_type(context, type.id());
  if (!meta_type) {
    return {error_domain::serialization, errc::not_found};
  }

  return deserialize(context, meta_type, &object, reader);
}

} // namespace detail

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象从指定后端读入。 */
template <typename T> [[nodiscard]] error deserialize(T& object, reader& reader) {
  serialization::context context{};
  const meta::type type = detail::find_registered_type<T>(context);
  if (!type.registered()) {
    return {error_domain::serialization, errc::missing_type};
  }

  const meta::type meta_type = detail::find_registered_type(context, type.id());
  if (!meta_type) {
    return {error_domain::serialization, errc::not_found};
  }

  return detail::deserialize(context, meta_type, &object, reader);
}

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象按上下文从指定后端读入。 */
template <typename T>
[[nodiscard]] error deserialize(const serialization::context& context, T& object, reader& reader) {
  const meta::type type = detail::find_registered_type<T>(context);
  if (!type.registered()) {
    return {error_domain::serialization, errc::missing_type};
  }

  const meta::type meta_type = detail::find_registered_type(context, type.id());
  if (!meta_type) {
    return {error_domain::serialization, errc::not_found};
  }

  return detail::deserialize(context, meta_type, &object, reader);
}

/** @brief 按上下文从序列化 DOM 中间值读回对象。 */
template <typename T>
[[nodiscard]] error read_value_from(const serialization::context& context, T& object,
                                    const variant& value) {
  return detail::read_dom_value_from(context, object, value);
}

/** @brief 从序列化 DOM 中间值读回对象。 */
template <typename T> [[nodiscard]] error read_value_from(T& object, const variant& value) {
  return read_value_from(serialization::context{}, object, value);
}

/** @brief 从 JSON 文本读回对象。 */
template <typename T> [[nodiscard]] error read_json_from(T& object, std::string_view text) {
  return detail::read_json_text_from(serialization::context{}, object, text, {});
}

/** @brief 按上下文从 JSON 文本读回对象。 */
template <typename T>
[[nodiscard]] error read_json_from(const serialization::context& context, T& object,
                                   std::string_view text) {
  return detail::read_json_text_from(context, object, text, {});
}

/** @brief 按 JSON 格式选项从 JSON 文本读回对象。 */
template <typename T>
[[nodiscard]] error read_json_from(T& object, std::string_view text, json_options options) {
  return detail::read_json_text_from(serialization::context{}, object, text, options);
}

/** @brief 按上下文和 JSON 格式选项从 JSON 文本读回对象。 */
template <typename T>
[[nodiscard]] error read_json_from(const serialization::context& context, T& object,
                                   std::string_view text, json_options options) {
  return detail::read_json_text_from(context, object, text, options);
}

/** @brief 从 JSON 输入流读回对象；当前复用 DOM parser 路径。 */
template <typename T>
[[nodiscard]] error read_json_from(T& object, io::input_stream& stream,
                                   memory_resource* resource = nullptr) {
  variant_result value = parse_json(stream, resource);
  if (value.failed()) {
    return value.status;
  }

  return read_value_from(object, value.value);
}

/** @brief 从 JSON 输入流按格式选项读回对象；当前复用 DOM parser 路径。 */
template <typename T>
[[nodiscard]] error read_json_from(T& object, io::input_stream& stream, json_options options,
                                   memory_resource* resource = nullptr) {
  variant_result value = parse_json(stream, options, resource);
  if (value.failed()) {
    return value.status;
  }

  return read_value_from(object, value.value);
}

/** @brief 按上下文从 JSON 输入流读回对象；当前复用 DOM parser 路径。 */
template <typename T>
[[nodiscard]] error read_json_from(const serialization::context& context, T& object,
                                   io::input_stream& stream, memory_resource* resource = nullptr) {
  variant_result value = parse_json(stream, json_options{}, resource);
  if (value.failed()) {
    return value.status;
  }

  return read_value_from(context, object, value.value);
}

/** @brief 按上下文和 JSON 格式选项从 JSON 输入流读回对象；当前复用 DOM parser 路径。 */
template <typename T>
[[nodiscard]] error read_json_from(const serialization::context& context, T& object,
                                   io::input_stream& stream, json_options options,
                                   memory_resource* resource = nullptr) {
  variant_result value = parse_json(stream, options, resource);
  if (value.failed()) {
    return value.status;
  }

  return read_value_from(context, object, value.value);
}

/** @brief 从文件系统路径读取 JSON 并读回对象；当前复用 DOM parser 路径。 */
template <typename T>
[[nodiscard]] error read_json_from(T& object, io::file_system& file_system, io::path_view path,
                                   memory_resource* resource = nullptr) {
  variant_result value = parse_json_from(file_system, path, resource);
  if (value.failed()) {
    return value.status;
  }

  return read_value_from(object, value.value);
}

/** @brief 从文件系统路径按格式选项读取 JSON 并读回对象；当前复用 DOM parser 路径。 */
template <typename T>
[[nodiscard]] error read_json_from(T& object, io::file_system& file_system, io::path_view path,
                                   json_options options, memory_resource* resource = nullptr) {
  variant_result value = parse_json_from(file_system, path, options, resource);
  if (value.failed()) {
    return value.status;
  }

  return read_value_from(object, value.value);
}

/** @brief 按上下文从文件系统路径读取 JSON 并读回对象；当前复用 DOM parser 路径。 */
template <typename T>
[[nodiscard]] error read_json_from(const serialization::context& context, T& object,
                                   io::file_system& file_system, io::path_view path,
                                   memory_resource* resource = nullptr) {
  variant_result value = parse_json_from(file_system, path, json_options{}, resource);
  if (value.failed()) {
    return value.status;
  }

  return read_value_from(context, object, value.value);
}

/** @brief 按上下文和格式选项从文件系统路径读取 JSON 并读回对象；当前复用 DOM parser 路径。 */
template <typename T>
[[nodiscard]] error read_json_from(const serialization::context& context, T& object,
                                   io::file_system& file_system, io::path_view path,
                                   json_options options, memory_resource* resource = nullptr) {
  variant_result value = parse_json_from(file_system, path, options, resource);
  if (value.failed()) {
    return value.status;
  }

  return read_value_from(context, object, value.value);
}

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象转换为序列化 DOM 中间值。 */
template <typename T> [[nodiscard]] variant_result to_value(const T& object) {
  serialization::context context{};
  const meta::type type = detail::find_registered_type<T>(context);
  if (!type.registered()) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_base_of_v<meta::object, value_type>) {
    if (detail::object_requires_polymorphic_envelope(type.id(), object)) {
      return detail::to_polymorphic_object_serialization_value(context, type.id(), object);
    }
  }

  const meta::type meta_type = detail::find_registered_type(context, type.id());
  if (!meta_type) {
    return {{error_domain::serialization, errc::not_found}, {}};
  }

  return detail::to_serialization_value(context, meta_type, &object);
}

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象按上下文转换为序列化 DOM 中间值。 */
template <typename T>
[[nodiscard]] variant_result to_value(const serialization::context& context, const T& object) {
  const meta::type type = detail::find_registered_type<T>(context);
  if (!type.registered()) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_base_of_v<meta::object, value_type>) {
    if (detail::object_requires_polymorphic_envelope(type.id(), object)) {
      return detail::to_polymorphic_object_serialization_value(context, type.id(), object);
    }
  }

  const meta::type meta_type = detail::find_registered_type(context, type.id());
  if (!meta_type) {
    return {{error_domain::serialization, errc::not_found}, {}};
  }

  return detail::to_serialization_value(context, meta_type, &object);
}

/** @brief 把对象写入简单文本 buffer。 */
template <typename T> [[nodiscard]] error write_text_to(const T& object, string& output) {
  serialization::text_writer writer{output};
  return serialize(object, writer);
}

/** @brief 把对象写入输出流；当前简单文本路径会先生成临时 buffer。 */
template <typename T>
[[nodiscard]] error write_text_to(const T& object, io::output_stream& output) {
  string buffer;
  const error status = write_text_to(object, buffer);
  if (status) {
    return status;
  }

  error written = output.write_all(std::string_view{buffer.data(), buffer.size()});
  if (written) {
    return written;
  }

  return output.flush();
}

/** @brief 按上下文把对象写入简单文本 buffer。 */
template <typename T>
[[nodiscard]] error write_text_to(const serialization::context& context, const T& object,
                                  string& output) {
  serialization::text_writer writer{output};
  return serialize(context, object, writer);
}

/** @brief 按上下文把对象写入输出流；当前简单文本路径会先生成临时 buffer。 */
template <typename T>
[[nodiscard]] error write_text_to(const serialization::context& context, const T& object,
                                  io::output_stream& output) {
  string buffer;
  const error status = write_text_to(context, object, buffer);
  if (status) {
    return status;
  }

  error written = output.write_all(std::string_view{buffer.data(), buffer.size()});
  if (written) {
    return written;
  }

  return output.flush();
}

/** @brief 把对象转换为简单文本。 */
template <typename T> [[nodiscard]] serialization_text_result to_text(const T& object) {
  string output;
  const error status = write_text_to(object, output);
  if (status) {
    return {status, {}};
  }

  return {{}, std::move(output)};
}

/** @brief 按上下文把对象转换为简单文本。 */
template <typename T>
[[nodiscard]] serialization_text_result to_text(const serialization::context& context,
                                                const T& object) {
  string output;
  const error status = write_text_to(context, object, output);
  if (status) {
    return {status, {}};
  }

  return {{}, std::move(output)};
}

/** @brief 把对象写入 JSON buffer。 */
template <typename T> [[nodiscard]] error write_json_to(const T& object, string& output) {
  serialization::json_writer writer{output};
  return serialize(object, writer);
}

/** @brief 把对象直接写入 JSON 输出流。 */
template <typename T>
[[nodiscard]] error write_json_to(const T& object, io::output_stream& output) {
  serialization::json_writer writer{output};
  const error status = serialize(object, writer);
  return status ? status : output.flush();
}

/** @brief 按 JSON 格式选项把对象写入 JSON buffer。 */
template <typename T>
[[nodiscard]] error write_json_to(const T& object, string& output, json_options options) {
  serialization::json_writer writer{output, options};
  return serialize(object, writer);
}

/** @brief 按 JSON 格式选项把对象直接写入 JSON 输出流。 */
template <typename T>
[[nodiscard]] error write_json_to(const T& object, io::output_stream& output,
                                  json_options options) {
  serialization::json_writer writer{output, options};
  const error status = serialize(object, writer);
  return status ? status : output.flush();
}

/** @brief 按上下文把对象写入 JSON buffer。 */
template <typename T>
[[nodiscard]] error write_json_to(const serialization::context& context, const T& object,
                                  string& output) {
  serialization::json_writer writer{output};
  return serialize(context, object, writer);
}

/** @brief 按上下文把对象直接写入 JSON 输出流。 */
template <typename T>
[[nodiscard]] error write_json_to(const serialization::context& context, const T& object,
                                  io::output_stream& output) {
  serialization::json_writer writer{output};
  const error status = serialize(context, object, writer);
  return status ? status : output.flush();
}

/** @brief 按上下文和 JSON 格式选项把对象写入 JSON buffer。 */
template <typename T>
[[nodiscard]] error write_json_to(const serialization::context& context, const T& object,
                                  string& output, json_options options) {
  serialization::json_writer writer{output, options};
  return serialize(context, object, writer);
}

/** @brief 按上下文和 JSON 格式选项把对象直接写入 JSON 输出流。 */
template <typename T>
[[nodiscard]] error write_json_to(const serialization::context& context, const T& object,
                                  io::output_stream& output, json_options options) {
  serialization::json_writer writer{output, options};
  const error status = serialize(context, object, writer);
  return status ? status : output.flush();
}

/** @brief 把对象转换为 JSON 字符串。 */
template <typename T> [[nodiscard]] serialization_text_result to_json(const T& object) {
  string output;
  const error status = write_json_to(object, output);
  if (status) {
    return {status, {}};
  }

  return {{}, std::move(output)};
}

/** @brief 按 JSON 格式选项把对象转换为 JSON 字符串。 */
template <typename T>
[[nodiscard]] serialization_text_result to_json(const T& object, json_options options) {
  string output;
  const error status = write_json_to(object, output, options);
  if (status) {
    return {status, {}};
  }

  return {{}, std::move(output)};
}

/** @brief 按上下文把对象转换为 JSON 字符串。 */
template <typename T>
[[nodiscard]] serialization_text_result to_json(const serialization::context& context,
                                                const T& object) {
  string output;
  const error status = write_json_to(context, object, output);
  if (status) {
    return {status, {}};
  }

  return {{}, std::move(output)};
}

/** @brief 按上下文和 JSON 格式选项把对象转换为 JSON 字符串。 */
template <typename T>
[[nodiscard]] serialization_text_result to_json(const serialization::context& context,
                                                const T& object, json_options options) {
  string output;
  const error status = write_json_to(context, object, output, options);
  if (status) {
    return {status, {}};
  }

  return {{}, std::move(output)};
}

/** @brief 把对象编码为 MessagePack 字节。 */
template <typename T> [[nodiscard]] io::byte_buffer_result to_messagepack(const T& object) {
  io::byte_buffer output = io::make_byte_buffer();
  messagepack_writer writer{output};
  const error status = serialize(object, writer);
  return {status, std::move(output)};
}

/** @brief 按上下文把对象编码为 MessagePack 字节。 */
template <typename T>
[[nodiscard]] io::byte_buffer_result to_messagepack(const serialization::context& context,
                                                    const T& object) {
  io::byte_buffer output = io::make_byte_buffer(context.effective_memory_resource());
  messagepack_writer writer{output, context.effective_temporary_resource()};
  const error status = serialize(context, object, writer);
  return {status, std::move(output)};
}

/** @brief 从一个完整 MessagePack 根值读回对象。 */
template <typename T>
[[nodiscard]] error read_messagepack_from(T& object, std::span<const std::byte> bytes,
                                          messagepack_options options = {}) {
  variant_result value = parse_messagepack(bytes, options);
  return value.failed() ? value.status : read_value_from(object, value.value);
}

/** @brief 按上下文从一个完整 MessagePack 根值读回对象。 */
template <typename T>
[[nodiscard]] error read_messagepack_from(const serialization::context& context, T& object,
                                          std::span<const std::byte> bytes,
                                          messagepack_options options = {}) {
  variant_result value = parse_messagepack(bytes, options, context.effective_memory_resource());
  return value.failed() ? value.status : read_value_from(context, object, value.value);
}

} // namespace caors::serialization

namespace caors {

using serialization::deserialize;
using serialization::read_json_from;
using serialization::read_messagepack_from;
using serialization::read_value_from;
using serialization::serialize;
using serialization::to_json;
using serialization::to_messagepack;
using serialization::to_value;

} // namespace caors

#endif
