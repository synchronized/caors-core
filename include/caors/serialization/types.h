// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_TYPES_H_
#define CAORS_SERIALIZATION_TYPES_H_

#include "caors/core/containers.h"
#include "caors/error.h"
#include "caors/meta.h"
#include "caors/object.h"
#include "caors/result.h"
#include "caors/value/variant.h"

#include <cstdint>
#include <initializer_list>
#include <string_view>

namespace caors {

/** @brief 序列化格式。当前 MVP 仅提供简单文本格式。 */
enum class serialization_format {
  text,
  json,
};

/** @brief 序列化后端能力。 */
enum class serialization_format_features : std::uint32_t {
  none = 0,
  named_fields = 1U << 0,
  numeric_fields = 1U << 1,
  ordered_fields = 1U << 2,
  self_describing = 1U << 3,
  schema_required = 1U << 4,
};

[[nodiscard]] constexpr serialization_format_features
operator|(serialization_format_features lhs, serialization_format_features rhs) noexcept {
  return static_cast<serialization_format_features>(static_cast<std::uint32_t>(lhs) |
                                                    static_cast<std::uint32_t>(rhs));
}

[[nodiscard]] constexpr serialization_format_features
operator&(serialization_format_features lhs, serialization_format_features rhs) noexcept {
  return static_cast<serialization_format_features>(static_cast<std::uint32_t>(lhs) &
                                                    static_cast<std::uint32_t>(rhs));
}

[[nodiscard]] constexpr bool has_feature(serialization_format_features features,
                                         serialization_format_features feature) noexcept {
  return (features & feature) != serialization_format_features::none;
}

/** @brief 单个字段的序列化配置。 */
struct serialization_field_data {
  string_name field_name{};
  string_name serialized_name{};
  std::uint32_t field_id = 0;
  std::uint32_t version_added = 0;
  std::uint32_t version_removed = 0;
  bool required = false;
  bool skipped = false;

  [[nodiscard]] constexpr bool valid() const noexcept { return !field_name.empty(); }
};

/**
 * @brief 序列化类型元信息。
 *
 * 字段配置按字段名引用反射字段。注册序列化组件时不要求反射字段已经注册，实际序列化时再解析
 * 字段配置和反射字段。
 */
struct serialization_type_data {
  serialization_format default_format = serialization_format::text;
  vector<serialization_field_data> fields{};

  [[nodiscard]] bool valid() const noexcept {
    if (default_format != serialization_format::text &&
        default_format != serialization_format::json) {
      return false;
    }

    for (const serialization_field_data& field : fields) {
      if (!field.valid()) {
        return false;
      }
    }

    return true;
  }
};

/** @brief 创建字段序列化配置。 */
[[nodiscard]] inline serialization_field_data
make_serialization_field(std::string_view field_name, std::string_view serialized_name = {},
                         bool skipped = false, std::uint32_t field_id = 0) {
  return serialization_field_data{
      string_name{field_name},
      serialized_name.empty() ? string_name{field_name} : string_name{serialized_name},
      field_id,
      0,
      0,
      false,
      skipped,
  };
}

/** @brief 创建类型序列化配置。 */
[[nodiscard]] inline serialization_type_data
make_serialization_type_data(std::initializer_list<serialization_field_data> fields) {
  return serialization_type_data{serialization_format::text,
                                 vector<serialization_field_data>{fields}};
}

using serialization_object_to_id_fn = object_id (*)(const object* instance, void* user_data);
using serialization_id_to_object_fn = object* (*)(object_id id, void* user_data);

/** @brief 对象引用解析器。 */
struct serialization_reference_resolver {
  serialization_object_to_id_fn object_to_id = nullptr;
  serialization_id_to_object_fn id_to_object = nullptr;
  void* user_data = nullptr;

  [[nodiscard]] constexpr bool can_write() const noexcept { return object_to_id != nullptr; }
  [[nodiscard]] constexpr bool can_read() const noexcept { return id_to_object != nullptr; }
};

/**
 * @brief 单次序列化调度上下文。
 *
 * `schema_version == 0` 表示不启用字段版本过滤。传入非 0 版本时，调度层只处理当前版本可见的
 * 字段。
 */
struct serialization_context {
  std::uint32_t schema_version = 0;
  const serialization_reference_resolver* reference_resolver = nullptr;
  /**
   * @brief `std::string_view` 反序列化使用的外部字符串存储。
   *
   * 默认保持 `nullptr`，此时读回 `std::string_view` 会返回 `errc::unsupported`，避免指向临时
   * 字符串。调用方如果能保证该存储的生命周期长于目标对象，可以传入一个可写字符串；读入时会把文本
   * 写入该字符串，再让目标 `std::string_view` 指向它。
   */
  string* string_view_storage = nullptr;
};

/** @brief 文本序列化结果。 */
using serialization_text_result = result<string>;

/** @brief 序列化中间值构造结果。 */
using variant_result = result<variant>;

} // namespace caors

#endif
