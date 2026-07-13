// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_OPERATIONS_H_
#define CAORS_SERIALIZATION_OPERATIONS_H_

#include "caors/object.h"
#include "caors/reflection/registry.h"
#include "caors/serialization/codec.h"
#include "caors/serialization/detail/operations.h"
#include "caors/serialization/dom_reader.h"
#include "caors/serialization/registry.h"
#include "caors/serialization/writer.h"

#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::serialization {

/** @brief 把已注册序列化和反射信息的 C++ 对象写入指定后端。 */
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

  const reflection::type reflection_type = reflection::find(type.id());
  if (!reflection_type) {
    return {error_domain::serialization, errc::not_found};
  }

  return detail::serialize(context, reflection_type, &object, writer);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文写入指定后端。 */
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

  const reflection::type reflection_type = reflection::find(type.id());
  if (!reflection_type) {
    return {error_domain::serialization, errc::not_found};
  }

  return detail::serialize(context, reflection_type, &object, writer);
}

/** @brief 将序列化中间值写入指定后端。 */
[[nodiscard]] CAORS_API error write_variant(writer& writer, const variant& value);

/** @brief 解析 JSON 文本为序列化中间值。 */
[[nodiscard]] CAORS_API variant_result parse_json(std::string_view text);

/** @brief 按格式选项解析 JSON 文本为序列化中间值。 */
[[nodiscard]] CAORS_API variant_result parse_json(std::string_view text, json_options options);

/** @brief 把已注册序列化和反射信息的 C++ 对象从指定后端读入。 */
template <typename T> [[nodiscard]] error deserialize(T& object, reader& reader) {
  const meta::type type = caors::meta::find<T>();
  if (!type.registered()) {
    return {error_domain::serialization, errc::missing_type};
  }

  const reflection::type reflection_type = reflection::find(type.id());
  if (!reflection_type) {
    return {error_domain::serialization, errc::not_found};
  }

  return detail::deserialize(serialization::context{}, reflection_type, &object, reader);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文从指定后端读入。 */
template <typename T>
[[nodiscard]] error deserialize(const serialization::context& context, T& object, reader& reader) {
  const meta::type type = caors::meta::find<T>();
  if (!type.registered()) {
    return {error_domain::serialization, errc::missing_type};
  }

  const reflection::type reflection_type = reflection::find(type.id());
  if (!reflection_type) {
    return {error_domain::serialization, errc::not_found};
  }

  return detail::deserialize(context, reflection_type, &object, reader);
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
template <typename T> [[nodiscard]] error deserialize_json(T& object, std::string_view text) {
  variant_result value = parse_json(text);
  if (value.failed()) {
    return value.status;
  }

  return deserialize(object, value.value);
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
  variant_result value = parse_json(text);
  if (value.failed()) {
    return value.status;
  }

  return deserialize(context, object, value.value);
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

/** @brief 把已注册序列化和反射信息的 C++ 对象转换为序列化中间值。 */
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

  const reflection::type reflection_type = reflection::find(type.id());
  if (!reflection_type) {
    return {{error_domain::serialization, errc::not_found}, {}};
  }

  return detail::to_serialization_value(context, reflection_type, &object);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文转换为序列化中间值。 */
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

  const reflection::type reflection_type = reflection::find(type.id());
  if (!reflection_type) {
    return {{error_domain::serialization, errc::not_found}, {}};
  }

  return detail::to_serialization_value(context, reflection_type, &object);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象序列化到外部简单文本 buffer。 */
template <typename T> [[nodiscard]] error serialize_text_to(const T& object, string& output) {
  serialization::text_writer writer{output};
  return serialize(object, writer);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文序列化到外部简单文本 buffer。 */
template <typename T>
[[nodiscard]] error serialize_text_to(const serialization::context& context, const T& object,
                                      string& output) {
  serialization::text_writer writer{output};
  return serialize(context, object, writer);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象序列化为简单文本格式。 */
template <typename T> [[nodiscard]] serialization_text_result serialize_text(const T& object) {
  string output;
  const error status = serialize_text_to(object, output);
  if (status) {
    return {status, {}};
  }

  return {{}, std::move(output)};
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文序列化为简单文本格式。 */
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

/** @brief 把已注册序列化和反射信息的 C++ 对象序列化到外部 JSON buffer。 */
template <typename T> [[nodiscard]] error serialize_json_to(const T& object, string& output) {
  serialization::json_writer writer{output};
  return serialize(object, writer);
}

/** @brief 按 JSON 格式选项把已注册序列化和反射信息的 C++ 对象序列化到外部 JSON buffer。 */
template <typename T>
[[nodiscard]] error serialize_json_to(const T& object, string& output, json_options options) {
  serialization::json_writer writer{output, options};
  return serialize(object, writer);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文序列化到外部 JSON buffer。 */
template <typename T>
[[nodiscard]] error serialize_json_to(const serialization::context& context, const T& object,
                                      string& output) {
  serialization::json_writer writer{output};
  return serialize(context, object, writer);
}

/** @brief 按上下文和 JSON 格式选项序列化到外部 JSON buffer。 */
template <typename T>
[[nodiscard]] error serialize_json_to(const serialization::context& context, const T& object,
                                      string& output, json_options options) {
  serialization::json_writer writer{output, options};
  return serialize(context, object, writer);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象序列化为 JSON。 */
template <typename T> [[nodiscard]] serialization_text_result serialize_json(const T& object) {
  string output;
  const error status = serialize_json_to(object, output);
  if (status) {
    return {status, {}};
  }

  return {{}, std::move(output)};
}

/** @brief 按 JSON 格式选项把已注册序列化和反射信息的 C++ 对象序列化为 JSON。 */
template <typename T>
[[nodiscard]] serialization_text_result serialize_json(const T& object, json_options options) {
  string output;
  const error status = serialize_json_to(object, output, options);
  if (status) {
    return {status, {}};
  }

  return {{}, std::move(output)};
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文序列化为 JSON。 */
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

/** @brief 按上下文和 JSON 格式选项把已注册序列化和反射信息的 C++ 对象序列化为 JSON。 */
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

/** @brief 兼容入口：把对象按真实运行期类型序列化为多态 DOM envelope。 */
[[nodiscard]] CAORS_API variant_result
to_polymorphic_serialization_value(const objects::object& object);

/** @brief 按上下文把对象按真实运行期类型序列化为多态 DOM envelope。 */
[[nodiscard]] CAORS_API variant_result to_polymorphic_serialization_value(
    const serialization::context& context, const objects::object& object);

/** @brief 兼容入口：把可空对象指针按真实运行期类型序列化为多态 DOM envelope；空指针写为 null。 */
[[nodiscard]] CAORS_API variant_result
to_polymorphic_serialization_value(const objects::object* object);

/** @brief 按上下文把可空对象指针按真实运行期类型序列化为多态 DOM envelope；空指针写为 null。 */
[[nodiscard]] CAORS_API variant_result to_polymorphic_serialization_value(
    const serialization::context& context, const objects::object* object);

/**
 * @brief 兼容入口：从多态 DOM envelope 构造对象。
 *
 * `declared_base` 是字段或调用点声明的对象基类类型。动态类型必须继承自该类型。成功返回的对象由
 * `context.factories.type_factory` 拥有；缺少类型工厂、类型不匹配或反序列化失败时返回错误。
 */
[[nodiscard]] CAORS_API result<objects::object*>
deserialize_polymorphic_object(const serialization::context& context,
                               objects::object_type_id declared_base, const variant& value);

/** @brief 兼容入口：按 C++ 基类类型从多态 DOM envelope 构造对象。 */
template <typename Base>
[[nodiscard]] result<Base*> deserialize_polymorphic_object(const serialization::context& context,
                                                           const variant& value) {
  static_assert(std::is_base_of_v<objects::object, Base>, "Base must derive from caors::object");

  const objects::type base_type = objects::find<Base>();
  if (!base_type) {
    return {{error_domain::serialization, errc::missing_type}, nullptr};
  }

  result<objects::object*> object = deserialize_polymorphic_object(context, base_type.id(), value);
  if (object.failed()) {
    return {object.status, nullptr};
  }

  Base* typed = dynamic_cast<Base*>(object.value);
  if (typed == nullptr) {
    if (const serialization::type_factory* factory = context.effective_type_factory()) {
      factory->destroy_type(object.value);
    }
    return {{error_domain::serialization, errc::invalid_type}, nullptr};
  }

  return {{}, typed};
}

/** @brief 兼容入口：把对象按真实运行期类型序列化为 JSON envelope。 */
[[nodiscard]] CAORS_API serialization_text_result
serialize_polymorphic_json(const objects::object& object);

/** @brief 按上下文把对象按真实运行期类型序列化为 JSON envelope。 */
[[nodiscard]] CAORS_API serialization_text_result
serialize_polymorphic_json(const serialization::context& context, const objects::object& object);

/** @brief 兼容入口：从 JSON envelope 构造多态对象。 */
[[nodiscard]] CAORS_API result<objects::object*>
deserialize_polymorphic_json(const serialization::context& context,
                             objects::object_type_id declared_base, std::string_view text);

/** @brief 兼容入口：按 C++ 基类类型从 JSON envelope 构造多态对象。 */
template <typename Base>
[[nodiscard]] result<Base*> deserialize_polymorphic_json(const serialization::context& context,
                                                         std::string_view text) {
  variant_result value = parse_json(text);
  if (value.failed()) {
    return {value.status, nullptr};
  }

  return deserialize_polymorphic_object<Base>(context, value.value);
}

} // namespace caors::serialization

namespace caors {

using serialization::deserialize;
using serialization::deserialize_json;
using serialization::parse_json;
using serialization::serialize;
using serialization::serialize_json;
using serialization::serialize_json_to;
using serialization::serialize_text;
using serialization::serialize_text_to;
using serialization::to_serialization_value;

} // namespace caors

#endif
