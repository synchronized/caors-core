// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_OPERATIONS_H_
#define CAORS_SERIALIZATION_OPERATIONS_H_

#include "caors/serialization/codec.h"
#include "caors/serialization/dom_reader.h"
#include "caors/serialization/registry.h"
#include "caors/serialization/writer.h"

#include <optional>

namespace caors {

/** @brief 通过反射类型视图和指定后端序列化对象。 */
[[nodiscard]] CAORS_API error serialize(const reflection_type& type, const void* object,
                                        serialization_writer& writer);

/** @brief 通过反射类型视图、指定后端和上下文序列化对象。 */
[[nodiscard]] CAORS_API error serialize(const reflection_type& type, const void* object,
                                        serialization_writer& writer,
                                        const serialization_context& context);

/** @brief 通过类型 id 查找反射信息并使用指定后端序列化对象。 */
[[nodiscard]] CAORS_API error serialize(type_id id, const void* object,
                                        serialization_writer& writer);

/** @brief 通过类型 id 查找反射信息并使用指定后端和上下文序列化对象。 */
[[nodiscard]] CAORS_API error serialize(type_id id, const void* object,
                                        serialization_writer& writer,
                                        const serialization_context& context);

/** @brief 把已注册序列化和反射信息的 C++ 对象写入指定后端。 */
template <typename T> [[nodiscard]] error serialize(const T& object, serialization_writer& writer) {
  const std::optional<meta_type> type = caors::find_type<T>();
  if (!type.has_value()) {
    return {error_domain::serialization, errc::missing_type};
  }

  return serialize(type->id(), &object, writer);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文写入指定后端。 */
template <typename T>
[[nodiscard]] error serialize(const T& object, serialization_writer& writer,
                              const serialization_context& context) {
  const std::optional<meta_type> type = caors::find_type<T>();
  if (!type.has_value()) {
    return {error_domain::serialization, errc::missing_type};
  }

  return serialize(type->id(), &object, writer, context);
}

/** @brief 将序列化中间值写入指定后端。 */
[[nodiscard]] CAORS_API error write(serialization_writer& writer, const variant& value);

/** @brief 通过反射类型视图和指定后端反序列化到对象。 */
[[nodiscard]] CAORS_API error deserialize(const reflection_type& type, void* object,
                                          serialization_reader& reader);

/** @brief 通过反射类型视图、指定后端和上下文反序列化到对象。 */
[[nodiscard]] CAORS_API error deserialize(const reflection_type& type, void* object,
                                          serialization_reader& reader,
                                          const serialization_context& context);

/** @brief 通过类型 id 查找反射信息并使用指定后端反序列化到对象。 */
[[nodiscard]] CAORS_API error deserialize(type_id id, void* object, serialization_reader& reader);

/** @brief 通过类型 id 查找反射信息并使用指定后端和上下文反序列化到对象。 */
[[nodiscard]] CAORS_API error deserialize(type_id id, void* object, serialization_reader& reader,
                                          const serialization_context& context);

/** @brief 把已注册序列化和反射信息的 C++ 对象从指定后端读入。 */
template <typename T> [[nodiscard]] error deserialize(T& object, serialization_reader& reader) {
  const std::optional<meta_type> type = caors::find_type<T>();
  if (!type.has_value()) {
    return {error_domain::serialization, errc::missing_type};
  }

  return deserialize(type->id(), &object, reader);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文从指定后端读入。 */
template <typename T>
[[nodiscard]] error deserialize(T& object, serialization_reader& reader,
                                const serialization_context& context) {
  const std::optional<meta_type> type = caors::find_type<T>();
  if (!type.has_value()) {
    return {error_domain::serialization, errc::missing_type};
  }

  return deserialize(type->id(), &object, reader, context);
}

/** @brief 从序列化 DOM 中间对象反序列化到对象。 */
template <typename T> [[nodiscard]] error deserialize(T& object, const variant& value) {
  variant_serialization_reader reader{value};
  return deserialize(object, reader);
}

/** @brief 按上下文从序列化 DOM 中间对象反序列化到对象。 */
template <typename T>
[[nodiscard]] error deserialize(T& object, const variant& value,
                                const serialization_context& context) {
  variant_serialization_reader reader{value};
  return deserialize(object, reader, context);
}

/** @brief 通过反射类型视图构造序列化中间值。 */
[[nodiscard]] CAORS_API variant_result to_serialization_value(const reflection_type& type,
                                                              const void* object);

/** @brief 通过反射类型视图和上下文构造序列化中间值。 */
[[nodiscard]] CAORS_API variant_result to_serialization_value(const reflection_type& type,
                                                              const void* object,
                                                              const serialization_context& context);

/** @brief 通过类型 id 查找反射信息并构造序列化中间值。 */
[[nodiscard]] CAORS_API variant_result to_serialization_value(type_id id, const void* object);

/** @brief 通过类型 id 查找反射信息并按上下文构造序列化中间值。 */
[[nodiscard]] CAORS_API variant_result to_serialization_value(type_id id, const void* object,
                                                              const serialization_context& context);

/** @brief 把已注册序列化和反射信息的 C++ 对象转换为序列化中间值。 */
template <typename T> [[nodiscard]] variant_result to_serialization_value(const T& object) {
  const std::optional<meta_type> type = caors::find_type<T>();
  if (!type.has_value()) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  return to_serialization_value(type->id(), &object);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文转换为序列化中间值。 */
template <typename T>
[[nodiscard]] variant_result to_serialization_value(const T& object,
                                                    const serialization_context& context) {
  const std::optional<meta_type> type = caors::find_type<T>();
  if (!type.has_value()) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  return to_serialization_value(type->id(), &object, context);
}

/** @brief 通过反射类型视图把对象序列化为简单文本格式。 */
[[nodiscard]] CAORS_API serialization_text_result serialize_text(const reflection_type& type,
                                                                 const void* object);

/** @brief 通过反射类型视图和上下文把对象序列化为简单文本格式。 */
[[nodiscard]] CAORS_API serialization_text_result serialize_text(
    const reflection_type& type, const void* object, const serialization_context& context);

/** @brief 通过类型 id 查找反射信息并把对象序列化为简单文本格式。 */
[[nodiscard]] CAORS_API serialization_text_result serialize_text(type_id id, const void* object);

/** @brief 通过类型 id 查找反射信息并按上下文序列化为简单文本格式。 */
[[nodiscard]] CAORS_API serialization_text_result
serialize_text(type_id id, const void* object, const serialization_context& context);

/** @brief 把已注册序列化和反射信息的 C++ 对象序列化为简单文本格式。 */
template <typename T> [[nodiscard]] serialization_text_result serialize_text(const T& object) {
  const std::optional<meta_type> type = caors::find_type<T>();
  if (!type.has_value()) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  return serialize_text(type->id(), &object);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文序列化为简单文本格式。 */
template <typename T>
[[nodiscard]] serialization_text_result serialize_text(const T& object,
                                                       const serialization_context& context) {
  const std::optional<meta_type> type = caors::find_type<T>();
  if (!type.has_value()) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  return serialize_text(type->id(), &object, context);
}

} // namespace caors

#endif
