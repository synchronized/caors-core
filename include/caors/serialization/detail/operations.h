// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_DETAIL_OPERATIONS_H_
#define CAORS_SERIALIZATION_DETAIL_OPERATIONS_H_

#include "caors/reflection/type.h"
#include "caors/serialization/reader.h"
#include "caors/serialization/types.h"
#include "caors/serialization/writer.h"

namespace caors::serialization::detail {

/** @brief 通过反射类型视图和指定后端序列化对象。 */
[[nodiscard]] CAORS_API error serialize(const reflection::type& type, const void* object,
                                        writer& writer);

/** @brief 通过反射类型视图、指定后端和上下文序列化对象。 */
[[nodiscard]] CAORS_API error serialize(const reflection::type& type, const void* object,
                                        writer& writer, const serialization::context& context);

/** @brief 通过类型 id 查找反射信息并使用指定后端序列化对象。 */
[[nodiscard]] CAORS_API error serialize(type_id id, const void* object, writer& writer);

/** @brief 通过类型 id 查找反射信息并使用指定后端和上下文序列化对象。 */
[[nodiscard]] CAORS_API error serialize(type_id id, const void* object, writer& writer,
                                        const serialization::context& context);

/** @brief 通过反射类型视图和指定后端反序列化到对象。 */
[[nodiscard]] CAORS_API error deserialize(const reflection::type& type, void* object,
                                          reader& reader);

/** @brief 通过反射类型视图、指定后端和上下文反序列化到对象。 */
[[nodiscard]] CAORS_API error deserialize(const reflection::type& type, void* object,
                                          reader& reader, const serialization::context& context);

/** @brief 通过类型 id 查找反射信息并使用指定后端反序列化到对象。 */
[[nodiscard]] CAORS_API error deserialize(type_id id, void* object, reader& reader);

/** @brief 通过类型 id 查找反射信息并使用指定后端和上下文反序列化到对象。 */
[[nodiscard]] CAORS_API error deserialize(type_id id, void* object, reader& reader,
                                          const serialization::context& context);

/** @brief 通过反射类型视图构造序列化中间值。 */
[[nodiscard]] CAORS_API variant_result to_serialization_value(const reflection::type& type,
                                                              const void* object);

/** @brief 通过反射类型视图和上下文构造序列化中间值。 */
[[nodiscard]] CAORS_API variant_result to_serialization_value(
    const reflection::type& type, const void* object, const serialization::context& context);

/** @brief 通过类型 id 查找反射信息并构造序列化中间值。 */
[[nodiscard]] CAORS_API variant_result to_serialization_value(type_id id, const void* object);

/** @brief 通过类型 id 查找反射信息并按上下文构造序列化中间值。 */
[[nodiscard]] CAORS_API variant_result
to_serialization_value(type_id id, const void* object, const serialization::context& context);

/** @brief 通过反射类型视图把对象序列化为简单文本格式。 */
[[nodiscard]] CAORS_API serialization_text_result serialize_text(const reflection::type& type,
                                                                 const void* object);

/** @brief 通过反射类型视图和上下文把对象序列化为简单文本格式。 */
[[nodiscard]] CAORS_API serialization_text_result serialize_text(
    const reflection::type& type, const void* object, const serialization::context& context);

/** @brief 通过类型 id 查找反射信息并把对象序列化为简单文本格式。 */
[[nodiscard]] CAORS_API serialization_text_result serialize_text(type_id id, const void* object);

/** @brief 通过类型 id 查找反射信息并按上下文序列化为简单文本格式。 */
[[nodiscard]] CAORS_API serialization_text_result
serialize_text(type_id id, const void* object, const serialization::context& context);

/** @brief 通过反射类型视图把对象序列化为 JSON。 */
[[nodiscard]] CAORS_API serialization_text_result serialize_json(const reflection::type& type,
                                                                 const void* object);

/** @brief 通过反射类型视图和上下文把对象序列化为 JSON。 */
[[nodiscard]] CAORS_API serialization_text_result serialize_json(
    const reflection::type& type, const void* object, const serialization::context& context);

/** @brief 通过类型 id 查找反射信息并把对象序列化为 JSON。 */
[[nodiscard]] CAORS_API serialization_text_result serialize_json(type_id id, const void* object);

/** @brief 通过类型 id 查找反射信息并按上下文序列化为 JSON。 */
[[nodiscard]] CAORS_API serialization_text_result
serialize_json(type_id id, const void* object, const serialization::context& context);

/** @brief 判断对象运行期类型是否需要写出多态 envelope。 */
[[nodiscard]] CAORS_API bool
object_requires_polymorphic_envelope(type_id declared_id, const objects::object& object) noexcept;

/** @brief 按对象运行期真实类型写出多态 envelope。 */
[[nodiscard]] CAORS_API error serialize_polymorphic_object(type_id declared_id,
                                                           const objects::object& object,
                                                           writer& writer,
                                                           const serialization::context& context);

/** @brief 按对象运行期真实类型构造多态 envelope。 */
[[nodiscard]] CAORS_API variant_result to_polymorphic_object_serialization_value(
    type_id declared_id, const objects::object& object, const serialization::context& context);

/** @brief 判断 DOM 值是否为多态 envelope。 */
[[nodiscard]] CAORS_API bool is_polymorphic_value(const variant& value) noexcept;

/** @brief 把多态 envelope 写入已存在且运行期类型匹配的对象。 */
[[nodiscard]] CAORS_API error
deserialize_polymorphic_object_into(type_id declared_id, objects::object& object,
                                    const variant& value, const serialization::context& context);

} // namespace caors::serialization::detail

#endif
