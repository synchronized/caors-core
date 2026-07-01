// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_CONTAINER_TRAITS_H_
#define CAORS_META_CONTAINER_TRAITS_H_

namespace caors {

/** @brief 顺序容器类型语义 traits。 */
template <typename T> struct sequence_traits {
  static constexpr bool is_sequence = false;
};

/** @brief 集合容器类型语义 traits。 */
template <typename T> struct set_traits {
  static constexpr bool is_set = false;
};

/** @brief 关联容器类型语义 traits。 */
template <typename T> struct associative_traits {
  static constexpr bool is_associative = false;
};

/** @brief 二元组类型语义 traits。 */
template <typename T> struct pair_traits {
  static constexpr bool is_pair = false;
};

/** @brief tuple-like 类型语义 traits。 */
template <typename T> struct tuple_traits {
  static constexpr bool is_tuple = false;
};

/** @brief variant-like 类型语义 traits。 */
template <typename T> struct variant_traits {
  static constexpr bool is_variant = false;
};

} // namespace caors

#endif
