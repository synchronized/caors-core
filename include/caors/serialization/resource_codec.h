// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_RESOURCE_CODEC_H_
#define CAORS_SERIALIZATION_RESOURCE_CODEC_H_

#include "caors/resources/ref.h"
#include "caors/serialization/codec.h"
#include "caors/serialization/types.h"

#include <string>
#include <string_view>

namespace caors::serialization::detail {

/** @brief 按字符串 GUID 写出资源标识。 */
[[nodiscard]] inline error write_resource_guid(writer& writer, resources::resource_guid guid) {
  const std::string text = guid.string();
  return writer.write_string(std::string_view{text.data(), text.size()});
}

/** @brief 从字符串 GUID 读入资源标识。 */
[[nodiscard]] inline error read_resource_guid(reader& reader, resources::resource_guid& guid) {
  string text;
  const error status = reader.read_string(text);
  if (status) {
    return status;
  }

  const auto parsed = resources::resource_guid::parse(std::string_view{text.data(), text.size()});
  if (!parsed.has_value()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  guid = *parsed;
  return {};
}

/** @brief 构造资源标识的序列化中间值。 */
[[nodiscard]] inline variant make_resource_guid_value(resources::resource_guid guid) {
  const std::string text = guid.string();
  return variant::from(std::string_view{text.data(), text.size()});
}

/** @brief 查询资源类型对应的运行期类型 id；类型未注册时返回 invalid。 */
template <typename T> [[nodiscard]] inline type_id resource_type_id() noexcept {
  const meta::type type = meta::find<T>();
  return type ? type.id() : type_id::invalid();
}

/** @brief 向上下文依赖收集器记录一个资源引用。 */
inline void collect_resource_dependency(const serialization::context& context,
                                        resources::resource_guid guid, type_id type) {
  if (context.resource_dependencies != nullptr && context.resource_dependencies->can_collect() &&
      guid) {
    context.resource_dependencies->collect(guid, type, context.resource_dependencies->user_data);
  }
}

/** @brief 按上下文策略解析资源引用。 */
template <typename T>
[[nodiscard]] inline error resolve_resource_reference(resources::ref<T>& value,
                                                      const serialization::context& context) {
  if (!value.has_guid() ||
      context.resource_resolution == resource_ref_resolution::keep_unresolved) {
    return {};
  }

  if (context.resource_resolver == nullptr || !context.resource_resolver->can_resolve()) {
    return context.resource_resolution == resource_ref_resolution::require_resolved
               ? error{error_domain::serialization, errc::invalid_argument}
               : error{};
  }

  resources::resource* resolved =
      context.resource_resolver->resolve(value.guid(), context.resource_resolver->user_data);
  if (resolved == nullptr) {
    value.unresolve();
    return context.resource_resolution == resource_ref_resolution::require_resolved
               ? error{error_domain::resources, errc::not_found}
               : error{};
  }

  T* typed = dynamic_cast<T*>(resolved);
  if (typed == nullptr) {
    value.unresolve();
    return {error_domain::resources, errc::invalid_type};
  }

  if (!value.resolve(typed)) {
    return {error_domain::resources, errc::inconsistent_type};
  }

  return {};
}

} // namespace caors::serialization::detail

namespace caors::serialization {

/** @brief `resource_guid` 的字符串 GUID 序列化 codec。 */
template <> struct codec<resources::resource_guid> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, resources::resource_guid value) {
    return detail::write_resource_guid(writer, value);
  }

  [[nodiscard]] static error read(reader& reader, resources::resource_guid& value) {
    return detail::read_resource_guid(reader, value);
  }

  [[nodiscard]] static variant_result make_value(resources::resource_guid value) {
    return {{}, detail::make_resource_guid_value(value)};
  }
};

/** @brief 资源引用按稳定 `resource_guid` 序列化，不序列化运行期资源指针。 */
template <typename T> struct codec<resources::ref<T>> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, const resources::ref<T>& value) {
    if (!value.has_guid()) {
      return writer.write_null();
    }

    return detail::write_resource_guid(writer, value.guid());
  }

  [[nodiscard]] static error write(writer& writer, const resources::ref<T>& value,
                                   const serialization::context& context) {
    if (value.has_guid()) {
      detail::collect_resource_dependency(context, value.guid(), detail::resource_type_id<T>());
    }

    return write(writer, value);
  }

  [[nodiscard]] static error read(reader& reader, resources::ref<T>& value) {
    if (reader.is_null()) {
      value.reset();
      return {};
    }

    resources::resource_guid guid{};
    const error status = detail::read_resource_guid(reader, guid);
    if (status) {
      return status;
    }

    value.reset(guid);
    return {};
  }

  [[nodiscard]] static error read(reader& reader, resources::ref<T>& value,
                                  const serialization::context& context) {
    const error status = read(reader, value);
    if (status || !value.has_guid()) {
      return status;
    }

    detail::collect_resource_dependency(context, value.guid(), detail::resource_type_id<T>());
    return detail::resolve_resource_reference(value, context);
  }

  [[nodiscard]] static variant_result make_value(const resources::ref<T>& value) {
    if (!value.has_guid()) {
      return {{}, variant::null()};
    }

    return {{}, detail::make_resource_guid_value(value.guid())};
  }

  [[nodiscard]] static variant_result make_value(const resources::ref<T>& value,
                                                 const serialization::context& context) {
    if (value.has_guid()) {
      detail::collect_resource_dependency(context, value.guid(), detail::resource_type_id<T>());
    }

    return make_value(value);
  }
};

} // namespace caors::serialization

#endif
