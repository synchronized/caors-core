// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_TYPES_H_
#define CAORS_SERIALIZATION_TYPES_H_

#include "caors/error.h"
#include "caors/meta.h"
#include "caors/object.h"
#include "caors/result.h"
#include "caors/serialization/detail/type_data.h"
#include "caors/value/variant.h"

#include <cstdint>

namespace caors::serialization {

using object_to_id_fn = object_id (*)(const object* instance, void* user_data);
using id_to_object_fn = object* (*)(object_id id, void* user_data);

/** @brief 对象引用解析器。 */
struct reference_resolver {
  object_to_id_fn object_to_id = nullptr;
  id_to_object_fn id_to_object = nullptr;
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
struct context {
  std::uint32_t schema_version = 0;
  const reference_resolver* reference_resolver = nullptr;
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

} // namespace caors::serialization

#endif
