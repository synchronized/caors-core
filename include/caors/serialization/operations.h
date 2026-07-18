// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_OPERATIONS_H_
#define CAORS_SERIALIZATION_OPERATIONS_H_

#include "caors/meta/query.h"
#include "caors/meta/type.h"
#include "caors/object.h"
#include "caors/serialization/codec.h"
#include "caors/serialization/detail/operations.h"
#include "caors/serialization/dom_reader.h"
#include "caors/serialization/registry.h"
#include "caors/serialization/writer.h"

#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::serialization {

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象写入指定后端。 */
template <typename T> [[nodiscard]] error serialize(const T& object, writer& writer) {
  serialization::context context{};
  const meta::type type = caors::meta::find<T>();
  if (!type.registered()) {
    return {error_domain::serialization, errc::missing_type};
  }

  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_base_of_v<objects::object, value_type>) {
    if (detail::object_requires_polymorphic_envelope(type.id(), object)) {
      return detail::serialize_polymorphic_object(context, type.id(), object, writer);
    }
  }

  const meta::type meta_type = meta::find(type.id());
  if (!meta_type) {
    return {error_domain::serialization, errc::not_found};
  }

  return detail::serialize(context, meta_type, &object, writer);
}

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象按上下文写入指定后端。 */
template <typename T>
[[nodiscard]] error serialize(const serialization::context& context, const T& object,
                              writer& writer) {
  const meta::type type = caors::meta::find<T>();
  if (!type.registered()) {
    return {error_domain::serialization, errc::missing_type};
  }

  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_base_of_v<objects::object, value_type>) {
    if (detail::object_requires_polymorphic_envelope(type.id(), object)) {
      return detail::serialize_polymorphic_object(context, type.id(), object, writer);
    }
  }

  const meta::type meta_type = meta::find(type.id());
  if (!meta_type) {
    return {error_domain::serialization, errc::not_found};
  }

  return detail::serialize(context, meta_type, &object, writer);
}

/** @brief 将序列化中间值写入指定后端。 */
[[nodiscard]] CAORS_API error write_variant(writer& writer, const variant& value);

/** @brief 解析 JSON 文本为序列化中间值。 */
[[nodiscard]] CAORS_API variant_result parse_json(std::string_view text);

/** @brief 按格式选项解析 JSON 文本为序列化中间值。 */
[[nodiscard]] CAORS_API variant_result parse_json(std::string_view text, json_options options);

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象从指定后端读入。 */
template <typename T> [[nodiscard]] error deserialize(T& object, reader& reader) {
  const meta::type type = caors::meta::find<T>();
  if (!type.registered()) {
    return {error_domain::serialization, errc::missing_type};
  }

  const meta::type meta_type = meta::find(type.id());
  if (!meta_type) {
    return {error_domain::serialization, errc::not_found};
  }

  return detail::deserialize(serialization::context{}, meta_type, &object, reader);
}

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象按上下文从指定后端读入。 */
template <typename T>
[[nodiscard]] error deserialize(const serialization::context& context, T& object, reader& reader) {
  const meta::type type = caors::meta::find<T>();
  if (!type.registered()) {
    return {error_domain::serialization, errc::missing_type};
  }

  const meta::type meta_type = meta::find(type.id());
  if (!meta_type) {
    return {error_domain::serialization, errc::not_found};
  }

  return detail::deserialize(context, meta_type, &object, reader);
}

/** @brief 从序列化 DOM 中间对象反序列化到对象。 */
template <typename T> [[nodiscard]] error deserialize(T& object, const variant& value) {
  return deserialize(serialization::context{}, object, value);
}

/** @brief 按上下文从序列化 DOM 中间对象反序列化到对象。 */
template <typename T>
[[nodiscard]] error deserialize(const serialization::context& context, T& object,
                                const variant& value) {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_base_of_v<objects::object, value_type>) {
    if (detail::is_polymorphic_value(context, value)) {
      const meta::type type = caors::meta::find<value_type>();
      if (!type.registered()) {
        return {error_domain::serialization, errc::missing_type};
      }

      return detail::deserialize_polymorphic_object_into(context, type.id(), object, value);
    }
  }

  serialization::variant_reader reader{value};
  return deserialize(context, object, reader);
}

/** @brief 从 JSON 文本反序列化到对象。 */
template <typename T> [[nodiscard]] error deserialize(T& object, std::string_view text) {
  variant_result value = parse_json(text);
  if (value.failed()) {
    return value.status;
  }

  return deserialize(object, value.value);
}

/** @brief 按上下文从 JSON 文本反序列化到对象。 */
template <typename T>
[[nodiscard]] error deserialize(const serialization::context& context, T& object,
                                std::string_view text) {
  variant_result value = parse_json(text, context.config.json);
  if (value.failed()) {
    return value.status;
  }

  return deserialize(context, object, value.value);
}

/** @brief 从 JSON 文本反序列化到对象。 */
template <typename T> [[nodiscard]] error deserialize_json(T& object, std::string_view text) {
  return deserialize(object, text);
}

/** @brief 按 JSON 格式选项从 JSON 文本反序列化到对象。 */
template <typename T>
[[nodiscard]] error deserialize_json(T& object, std::string_view text, json_options options) {
  variant_result value = parse_json(text, options);
  if (value.failed()) {
    return value.status;
  }

  return deserialize(object, value.value);
}

/** @brief 按上下文从 JSON 文本反序列化到对象。 */
template <typename T>
[[nodiscard]] error deserialize_json(const serialization::context& context, T& object,
                                     std::string_view text) {
  return deserialize(context, object, text);
}

/** @brief 按上下文和 JSON 格式选项从 JSON 文本反序列化到对象。 */
template <typename T>
[[nodiscard]] error deserialize_json(const serialization::context& context, T& object,
                                     std::string_view text, json_options options) {
  variant_result value = parse_json(text, options);
  if (value.failed()) {
    return value.status;
  }

  return deserialize(context, object, value.value);
}

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象转换为序列化中间值。 */
template <typename T> [[nodiscard]] variant_result to_serialization_value(const T& object) {
  serialization::context context{};
  const meta::type type = caors::meta::find<T>();
  if (!type.registered()) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_base_of_v<objects::object, value_type>) {
    if (detail::object_requires_polymorphic_envelope(type.id(), object)) {
      return detail::to_polymorphic_object_serialization_value(context, type.id(), object);
    }
  }

  const meta::type meta_type = meta::find(type.id());
  if (!meta_type) {
    return {{error_domain::serialization, errc::not_found}, {}};
  }

  return detail::to_serialization_value(context, meta_type, &object);
}

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象按上下文转换为序列化中间值。 */
template <typename T>
[[nodiscard]] variant_result to_serialization_value(const serialization::context& context,
                                                    const T& object) {
  const meta::type type = caors::meta::find<T>();
  if (!type.registered()) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_base_of_v<objects::object, value_type>) {
    if (detail::object_requires_polymorphic_envelope(type.id(), object)) {
      return detail::to_polymorphic_object_serialization_value(context, type.id(), object);
    }
  }

  const meta::type meta_type = meta::find(type.id());
  if (!meta_type) {
    return {{error_domain::serialization, errc::not_found}, {}};
  }

  return detail::to_serialization_value(context, meta_type, &object);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象转换为序列化中间值。 */
template <typename T> [[nodiscard]] variant_result to_value(const T& object) {
  return to_serialization_value(object);
}

/** @brief 按上下文把 C++ 对象转换为序列化中间值。 */
template <typename T>
[[nodiscard]] variant_result to_value(const serialization::context& context, const T& object) {
  return to_serialization_value(context, object);
}

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象序列化到外部简单文本 buffer。 */
template <typename T> [[nodiscard]] error serialize_text_to(const T& object, string& output) {
  serialization::text_writer writer{output};
  return serialize(object, writer);
}

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象按上下文序列化到外部简单文本 buffer。 */
template <typename T>
[[nodiscard]] error serialize_text_to(const serialization::context& context, const T& object,
                                      string& output) {
  serialization::text_writer writer{output};
  return serialize(context, object, writer);
}

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象序列化为简单文本格式。 */
template <typename T> [[nodiscard]] serialization_text_result serialize_text(const T& object) {
  string output;
  const error status = serialize_text_to(object, output);
  if (status) {
    return {status, {}};
  }

  return {{}, std::move(output)};
}

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象按上下文序列化为简单文本格式。 */
template <typename T>
[[nodiscard]] serialization_text_result serialize_text(const serialization::context& context,
                                                       const T& object) {
  string output;
  const error status = serialize_text_to(context, object, output);
  if (status) {
    return {status, {}};
  }

  return {{}, std::move(output)};
}

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象序列化到外部 JSON buffer。 */
template <typename T> [[nodiscard]] error serialize_json_to(const T& object, string& output) {
  serialization::json_writer writer{output};
  return serialize(object, writer);
}

/** @brief 按 JSON 格式选项把已注册序列化和 meta 字段信息的 C++ 对象序列化到外部 JSON buffer。 */
template <typename T>
[[nodiscard]] error serialize_json_to(const T& object, string& output, json_options options) {
  serialization::json_writer writer{output, options};
  return serialize(object, writer);
}

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象按上下文序列化到外部 JSON buffer。 */
template <typename T>
[[nodiscard]] error serialize_json_to(const serialization::context& context, const T& object,
                                      string& output) {
  serialization::json_writer writer{output, context.config.json};
  return serialize(context, object, writer);
}

/** @brief 按上下文和 JSON 格式选项序列化到外部 JSON buffer。 */
template <typename T>
[[nodiscard]] error serialize_json_to(const serialization::context& context, const T& object,
                                      string& output, json_options options) {
  serialization::json_writer writer{output, options};
  return serialize(context, object, writer);
}

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象序列化为 JSON。 */
template <typename T> [[nodiscard]] serialization_text_result serialize_json(const T& object) {
  string output;
  const error status = serialize_json_to(object, output);
  if (status) {
    return {status, {}};
  }

  return {{}, std::move(output)};
}

/** @brief 按 JSON 格式选项把已注册序列化和 meta 字段信息的 C++ 对象序列化为 JSON。 */
template <typename T>
[[nodiscard]] serialization_text_result serialize_json(const T& object, json_options options) {
  string output;
  const error status = serialize_json_to(object, output, options);
  if (status) {
    return {status, {}};
  }

  return {{}, std::move(output)};
}

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象按上下文序列化为 JSON。 */
template <typename T>
[[nodiscard]] serialization_text_result serialize_json(const serialization::context& context,
                                                       const T& object) {
  string output;
  const error status = serialize_json_to(context, object, output);
  if (status) {
    return {status, {}};
  }

  return {{}, std::move(output)};
}

/** @brief 按上下文和 JSON 格式选项把已注册序列化和 meta 字段信息的 C++ 对象序列化为 JSON。 */
template <typename T>
[[nodiscard]] serialization_text_result serialize_json(const serialization::context& context,
                                                       const T& object, json_options options) {
  string output;
  const error status = serialize_json_to(context, object, output, options);
  if (status) {
    return {status, {}};
  }

  return {{}, std::move(output)};
}

/** @brief 把已注册序列化和 meta 字段信息的 C++ 对象序列化为 JSON 字符串。 */
template <typename T> [[nodiscard]] serialization_text_result to_string(const T& object) {
  return serialize_json(object);
}

/** @brief 按 JSON 格式选项把 C++ 对象序列化为 JSON 字符串。 */
template <typename T>
[[nodiscard]] serialization_text_result to_string(const T& object, json_options options) {
  return serialize_json(object, options);
}

/** @brief 按上下文把 C++ 对象序列化为 JSON 字符串。 */
template <typename T>
[[nodiscard]] serialization_text_result to_string(const serialization::context& context,
                                                  const T& object) {
  return serialize_json(context, object);
}

/** @brief 按上下文和 JSON 格式选项把 C++ 对象序列化为 JSON 字符串。 */
template <typename T>
[[nodiscard]] serialization_text_result to_string(const serialization::context& context,
                                                  const T& object, json_options options) {
  return serialize_json(context, object, options);
}

} // namespace caors::serialization

namespace caors {

using serialization::deserialize;
using serialization::serialize;
using serialization::to_string;
using serialization::to_value;

} // namespace caors

#endif
