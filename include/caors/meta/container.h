// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_CONTAINER_H_
#define CAORS_META_CONTAINER_H_

#include "caors/core/containers.h"
#include "caors/meta/type_id.h"

#include <cstddef>

namespace caors::meta {

using set_raw_visitor = bool (*)(const void*, void*);
using associative_raw_visitor = bool (*)(const void*, const void*, void*);

using sequence_size_fn = std::size_t (*)(const void*) noexcept;
using sequence_const_element_fn = const void* (*)(const void*, std::size_t) noexcept;
using sequence_mutable_element_fn = void* (*)(void*, std::size_t) noexcept;
using sequence_clear_fn = void (*)(void*) noexcept;
using sequence_resize_fn = bool (*)(void*, std::size_t);
using sequence_append_fn = bool (*)(void*, const void*);

using set_size_fn = std::size_t (*)(const void*) noexcept;
using set_visit_fn = bool (*)(const void*, set_raw_visitor, void*);
using set_clear_fn = void (*)(void*) noexcept;
using set_insert_fn = bool (*)(void*, const void*);
using container_iterator_create_fn = void* (*)(const void*) noexcept;
using container_iterator_clone_fn = void* (*)(const void*) noexcept;
using container_iterator_destroy_fn = void (*)(void*) noexcept;
using container_iterator_advance_fn = void (*)(void*) noexcept;
using container_iterator_equal_fn = bool (*)(const void*, const void*) noexcept;
using set_iterator_value_fn = const void* (*)(const void*) noexcept;

using associative_size_fn = std::size_t (*)(const void*) noexcept;
using associative_visit_fn = bool (*)(const void*, associative_raw_visitor, void*);
using associative_clear_fn = void (*)(void*) noexcept;
using associative_insert_fn = bool (*)(void*, const void*, const void*);
using associative_iterator_key_fn = const void* (*)(const void*) noexcept;
using associative_iterator_value_fn = const void* (*)(const void*) noexcept;

using pair_const_value_fn = const void* (*)(const void*) noexcept;
using pair_mutable_value_fn = void* (*)(void*) noexcept;
using tuple_const_element_fn = const void* (*)(const void*) noexcept;
using tuple_mutable_element_fn = void* (*)(void*) noexcept;
using variant_index_fn = std::size_t (*)(const void*) noexcept;
using variant_const_alternative_fn = const void* (*)(const void*) noexcept;
using variant_mutable_alternative_fn = void* (*)(void*) noexcept;
using variant_emplace_fn = bool (*)(void*);

/** @brief 顺序容器运行期访问数据。 */
struct sequence_data {
  type_id value_type{};
  sequence_size_fn size = nullptr;
  sequence_const_element_fn element = nullptr;
  sequence_mutable_element_fn mutable_element = nullptr;
  sequence_clear_fn clear = nullptr;
  sequence_resize_fn resize = nullptr;
  sequence_append_fn append = nullptr;
  bool can_read = true;
  bool read_in_place = false;

  [[nodiscard]] bool valid() const noexcept {
    return value_type && size != nullptr && element != nullptr;
  }
};

/** @brief 集合容器运行期访问数据。 */
struct set_data {
  type_id value_type{};
  set_size_fn size = nullptr;
  set_visit_fn visit = nullptr;
  set_clear_fn clear = nullptr;
  set_insert_fn insert = nullptr;
  container_iterator_create_fn begin = nullptr;
  container_iterator_create_fn end = nullptr;
  container_iterator_clone_fn iterator_clone = nullptr;
  container_iterator_destroy_fn iterator_destroy = nullptr;
  container_iterator_advance_fn iterator_advance = nullptr;
  container_iterator_equal_fn iterator_equal = nullptr;
  set_iterator_value_fn iterator_value = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return value_type && size != nullptr && visit != nullptr;
  }
};

/** @brief 关联容器运行期访问数据。 */
struct associative_data {
  type_id key_type{};
  type_id value_type{};
  associative_size_fn size = nullptr;
  associative_visit_fn visit = nullptr;
  associative_clear_fn clear = nullptr;
  associative_insert_fn insert = nullptr;
  container_iterator_create_fn begin = nullptr;
  container_iterator_create_fn end = nullptr;
  container_iterator_clone_fn iterator_clone = nullptr;
  container_iterator_destroy_fn iterator_destroy = nullptr;
  container_iterator_advance_fn iterator_advance = nullptr;
  container_iterator_equal_fn iterator_equal = nullptr;
  associative_iterator_key_fn iterator_key = nullptr;
  associative_iterator_value_fn iterator_value = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return key_type && value_type && size != nullptr && visit != nullptr;
  }
};

/** @brief 二元组运行期访问数据。 */
struct pair_data {
  type_id key_type{};
  type_id value_type{};
  pair_const_value_fn first = nullptr;
  pair_mutable_value_fn mutable_first = nullptr;
  pair_const_value_fn second = nullptr;
  pair_mutable_value_fn mutable_second = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return key_type && value_type && first != nullptr && second != nullptr;
  }
};

/** @brief tuple-like 单个元素运行期访问数据。 */
struct tuple_element_data {
  type_id value_type{};
  tuple_const_element_fn element = nullptr;
  tuple_mutable_element_fn mutable_element = nullptr;

  [[nodiscard]] bool valid() const noexcept { return value_type && element != nullptr; }
};

/** @brief tuple-like 运行期访问数据。 */
struct tuple_data {
  vector<tuple_element_data> elements{};

  [[nodiscard]] bool valid() const noexcept {
    for (const tuple_element_data& element : elements) {
      if (!element.valid()) {
        return false;
      }
    }

    return true;
  }
};

/** @brief variant-like 单个候选类型运行期访问数据。 */
struct variant_alternative_data {
  type_id value_type{};
  variant_const_alternative_fn alternative = nullptr;
  variant_mutable_alternative_fn mutable_alternative = nullptr;
  variant_emplace_fn emplace = nullptr;

  [[nodiscard]] bool valid() const noexcept { return value_type && alternative != nullptr; }
};

/** @brief variant-like 运行期访问数据。 */
struct variant_data {
  vector<variant_alternative_data> alternatives{};
  variant_index_fn index = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    if (index == nullptr || alternatives.empty()) {
      return false;
    }

    for (const variant_alternative_data& alternative : alternatives) {
      if (!alternative.valid()) {
        return false;
      }
    }

    return true;
  }
};

} // namespace caors::meta

#endif
