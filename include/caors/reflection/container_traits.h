// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_CONTAINER_TRAITS_H_
#define CAORS_REFLECTION_CONTAINER_TRAITS_H_

namespace caors {

/**
 * @brief 顺序容器反射 traits。
 *
 * 自定义顺序容器可以特化该模板接入反射系统。traits 负责提供元素类型、类型名前缀和基础访问函数。
 */
template <typename T> struct sequence_traits {
  static constexpr bool is_sequence = false;
};

/**
 * @brief 集合容器反射 traits。
 *
 * 自定义 set 或 hash_set 容器可以特化该模板接入反射和序列化系统。traits 负责提供元素类型、
 * 类型名前缀、遍历和插入入口。
 */
template <typename T> struct set_traits {
  static constexpr bool is_set = false;
};

/**
 * @brief 关联容器反射 traits。
 *
 * 自定义字典或 map 容器可以特化该模板接入反射和序列化系统。traits 负责提供 key/value
 * 类型、类型名前缀、遍历和插入入口。
 */
template <typename T> struct associative_traits {
  static constexpr bool is_associative = false;
};

} // namespace caors

#endif
