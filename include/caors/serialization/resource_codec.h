// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_RESOURCE_CODEC_H_
#define CAORS_SERIALIZATION_RESOURCE_CODEC_H_

#include "caors/resources/ref.h"
#include "caors/serialization/codec.h"

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

  [[nodiscard]] static variant_result make_value(const resources::ref<T>& value) {
    if (!value.has_guid()) {
      return {{}, variant::null()};
    }

    return {{}, detail::make_resource_guid_value(value.guid())};
  }
};

} // namespace caors::serialization

#endif
