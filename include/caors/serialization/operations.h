// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_OPERATIONS_H_
#define CAORS_SERIALIZATION_OPERATIONS_H_

#include "caors/serialization/codec.h"
#include "caors/serialization/detail/operations.h"
#include "caors/serialization/dom_reader.h"
#include "caors/serialization/registry.h"
#include "caors/serialization/writer.h"

#include <string_view>
#include <type_traits>

namespace caors::serialization {

/** @brief 把已注册序列化和反射信息的 C++ 对象写入指定后端。 */
template <typename T> [[nodiscard]] error serialize(const T& object, writer& writer) {
  const meta::type type = caors::meta::find<T>();
  if (!type) {
    return {error_domain::serialization, errc::missing_type};
  }

  return detail::serialize(type.id(), &object, writer);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文写入指定后端。 */
template <typename T>
[[nodiscard]] error serialize(const T& object, writer& writer,
                              const serialization::context& context) {
  const meta::type type = caors::meta::find<T>();
  if (!type) {
    return {error_domain::serialization, errc::missing_type};
  }

  return detail::serialize(type.id(), &object, writer, context);
}

/** @brief 将序列化中间值写入指定后端。 */
[[nodiscard]] CAORS_API error write_variant(writer& writer, const variant& value);

/** @brief 解析 JSON 文本为序列化中间值。 */
[[nodiscard]] CAORS_API variant_result parse_json(std::string_view text);

/** @brief 把已注册序列化和反射信息的 C++ 对象从指定后端读入。 */
template <typename T> [[nodiscard]] error deserialize(T& object, reader& reader) {
  const meta::type type = caors::meta::find<T>();
  if (!type) {
    return {error_domain::serialization, errc::missing_type};
  }

  return detail::deserialize(type.id(), &object, reader);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文从指定后端读入。 */
template <typename T>
[[nodiscard]] error deserialize(T& object, reader& reader, const serialization::context& context) {
  const meta::type type = caors::meta::find<T>();
  if (!type) {
    return {error_domain::serialization, errc::missing_type};
  }

  return detail::deserialize(type.id(), &object, reader, context);
}

/** @brief 从序列化 DOM 中间对象反序列化到对象。 */
template <typename T> [[nodiscard]] error deserialize(T& object, const variant& value) {
  serialization::variant_reader reader{value};
  return deserialize(object, reader);
}

/** @brief 按上下文从序列化 DOM 中间对象反序列化到对象。 */
template <typename T>
[[nodiscard]] error deserialize(T& object, const variant& value,
                                const serialization::context& context) {
  serialization::variant_reader reader{value};
  return deserialize(object, reader, context);
}

/** @brief 从 JSON 文本反序列化到对象。 */
template <typename T> [[nodiscard]] error deserialize_json(T& object, std::string_view text) {
  variant_result value = parse_json(text);
  if (value.failed()) {
    return value.status;
  }

  return deserialize(object, value.value);
}

/** @brief 按上下文从 JSON 文本反序列化到对象。 */
template <typename T>
[[nodiscard]] error deserialize_json(T& object, std::string_view text,
                                     const serialization::context& context) {
  variant_result value = parse_json(text);
  if (value.failed()) {
    return value.status;
  }

  return deserialize(object, value.value, context);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象转换为序列化中间值。 */
template <typename T> [[nodiscard]] variant_result to_serialization_value(const T& object) {
  const meta::type type = caors::meta::find<T>();
  if (!type) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  return detail::to_serialization_value(type.id(), &object);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文转换为序列化中间值。 */
template <typename T>
[[nodiscard]] variant_result to_serialization_value(const T& object,
                                                    const serialization::context& context) {
  const meta::type type = caors::meta::find<T>();
  if (!type) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  return detail::to_serialization_value(type.id(), &object, context);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象序列化为简单文本格式。 */
template <typename T> [[nodiscard]] serialization_text_result serialize_text(const T& object) {
  const meta::type type = caors::meta::find<T>();
  if (!type) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  return detail::serialize_text(type.id(), &object);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文序列化为简单文本格式。 */
template <typename T>
[[nodiscard]] serialization_text_result serialize_text(const T& object,
                                                       const serialization::context& context) {
  const meta::type type = caors::meta::find<T>();
  if (!type) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  return detail::serialize_text(type.id(), &object, context);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象序列化为 JSON。 */
template <typename T> [[nodiscard]] serialization_text_result serialize_json(const T& object) {
  const meta::type type = caors::meta::find<T>();
  if (!type) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  return detail::serialize_json(type.id(), &object);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文序列化为 JSON。 */
template <typename T>
[[nodiscard]] serialization_text_result serialize_json(const T& object,
                                                       const serialization::context& context) {
  const meta::type type = caors::meta::find<T>();
  if (!type) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  return detail::serialize_json(type.id(), &object, context);
}

/** @brief 把对象按真实运行期类型序列化为带 `$type` / `$value` 的多态 DOM envelope。 */
[[nodiscard]] CAORS_API variant_result to_polymorphic_serialization_value(
    const objects::object& object, const serialization::context& context = {});

/** @brief 把可空对象指针按真实运行期类型序列化为多态 DOM envelope；空指针写为 null。 */
[[nodiscard]] CAORS_API variant_result to_polymorphic_serialization_value(
    const objects::object* object, const serialization::context& context = {});

/**
 * @brief 从多态 DOM envelope 构造对象。
 *
 * `declared_base` 是字段或调用点声明的对象基类类型。动态类型必须继承自该类型。成功返回的对象由
 * `context.object_manager` 拥有；缺少 manager、类型不匹配或反序列化失败时返回错误。
 */
[[nodiscard]] CAORS_API result<objects::object*>
deserialize_polymorphic_object(objects::object_type_id declared_base, const variant& value,
                               const serialization::context& context);

/** @brief 按 C++ 基类类型从多态 DOM envelope 构造对象。 */
template <typename Base>
[[nodiscard]] result<Base*> deserialize_polymorphic_object(const variant& value,
                                                           const serialization::context& context) {
  static_assert(std::is_base_of_v<objects::object, Base>, "Base must derive from caors::object");

  const objects::type base_type = objects::find<Base>();
  if (!base_type) {
    return {{error_domain::serialization, errc::missing_type}, nullptr};
  }

  result<objects::object*> object = deserialize_polymorphic_object(base_type.id(), value, context);
  if (object.failed()) {
    return {object.status, nullptr};
  }

  Base* typed = dynamic_cast<Base*>(object.value);
  if (typed == nullptr) {
    if (context.object_manager != nullptr) {
      (void)context.object_manager->destroy(object.value);
    }
    return {{error_domain::serialization, errc::invalid_type}, nullptr};
  }

  return {{}, typed};
}

/** @brief 把对象按真实运行期类型序列化为 JSON envelope。 */
[[nodiscard]] CAORS_API serialization_text_result serialize_polymorphic_json(
    const objects::object& object, const serialization::context& context = {});

/** @brief 从 JSON envelope 构造多态对象。 */
[[nodiscard]] CAORS_API result<objects::object*>
deserialize_polymorphic_json(objects::object_type_id declared_base, std::string_view text,
                             const serialization::context& context);

/** @brief 按 C++ 基类类型从 JSON envelope 构造多态对象。 */
template <typename Base>
[[nodiscard]] result<Base*> deserialize_polymorphic_json(std::string_view text,
                                                         const serialization::context& context) {
  variant_result value = parse_json(text);
  if (value.failed()) {
    return {value.status, nullptr};
  }

  return deserialize_polymorphic_object<Base>(value.value, context);
}

} // namespace caors::serialization

namespace caors {

using serialization::deserialize;
using serialization::deserialize_json;
using serialization::deserialize_polymorphic_json;
using serialization::deserialize_polymorphic_object;
using serialization::parse_json;
using serialization::serialize;
using serialization::serialize_json;
using serialization::serialize_polymorphic_json;
using serialization::serialize_text;
using serialization::to_polymorphic_serialization_value;
using serialization::to_serialization_value;

} // namespace caors

#endif
