// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_JSON_OPTIONS_H_
#define CAORS_SERIALIZATION_JSON_OPTIONS_H_

#include <cstdint>

namespace caors::serialization {

/** @brief JSON 对象遇到重复 key 时的解析策略。 */
enum class json_duplicate_key_policy {
  /** @brief 保留所有同名成员，维持 DOM 原始形态。 */
  keep_all,
  /** @brief 保留第一项，后续同名成员会被忽略。 */
  use_first,
  /** @brief 保留最后一项，后续同名成员会覆盖前项。 */
  use_last,
  /** @brief 遇到重复 key 直接解析失败。 */
  reject,
};

/** @brief JSON reader、writer 和 parser 的格式层选项。 */
struct json_options {
  /** @brief 是否输出换行和缩进；默认输出紧凑 JSON。 */
  bool pretty = false;
  /** @brief `pretty` 开启时每一层使用的空格数。 */
  std::uint32_t indent = 2;
  /** @brief parser 允许的最大对象 / 数组嵌套深度；0 表示不限制。 */
  std::uint32_t max_parse_depth = 64;
  /** @brief parser 是否允许对象和数组末尾多一个逗号。 */
  bool allow_trailing_commas = false;
  /** @brief parser 遇到对象重复 key 时的处理策略。 */
  json_duplicate_key_policy duplicate_key_policy = json_duplicate_key_policy::keep_all;
};

} // namespace caors::serialization

#endif
