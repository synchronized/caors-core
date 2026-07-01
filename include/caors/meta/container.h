// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_CONTAINER_H_
#define CAORS_META_CONTAINER_H_

#include "caors/meta/type_id.h"

#include <cstddef>

namespace caors {

using meta_set_raw_visitor = bool (*)(const void*, void*);
using meta_associative_raw_visitor = bool (*)(const void*, const void*, void*);

using meta_sequence_size_fn = std::size_t (*)(const void*) noexcept;
using meta_sequence_const_element_fn = const void* (*)(const void*, std::size_t) noexcept;
using meta_sequence_mutable_element_fn = void* (*)(void*, std::size_t) noexcept;
using meta_sequence_clear_fn = void (*)(void*) noexcept;
using meta_sequence_resize_fn = bool (*)(void*, std::size_t);
using meta_sequence_append_fn = bool (*)(void*, const void*);

using meta_set_size_fn = std::size_t (*)(const void*) noexcept;
using meta_set_visit_fn = bool (*)(const void*, meta_set_raw_visitor, void*);
using meta_set_clear_fn = void (*)(void*) noexcept;
using meta_set_insert_fn = bool (*)(void*, const void*);
using meta_container_iterator_create_fn = void* (*)(const void*) noexcept;
using meta_container_iterator_clone_fn = void* (*)(const void*) noexcept;
using meta_container_iterator_destroy_fn = void (*)(void*) noexcept;
using meta_container_iterator_advance_fn = void (*)(void*) noexcept;
using meta_container_iterator_equal_fn = bool (*)(const void*, const void*) noexcept;
using meta_set_iterator_value_fn = const void* (*)(const void*) noexcept;

using meta_associative_size_fn = std::size_t (*)(const void*) noexcept;
using meta_associative_visit_fn = bool (*)(const void*, meta_associative_raw_visitor, void*);
using meta_associative_clear_fn = void (*)(void*) noexcept;
using meta_associative_insert_fn = bool (*)(void*, const void*, const void*);
using meta_associative_iterator_key_fn = const void* (*)(const void*) noexcept;
using meta_associative_iterator_value_fn = const void* (*)(const void*) noexcept;

/** @brief 顺序容器运行期访问数据。 */
struct meta_sequence_data {
  type_id value_type{};
  meta_sequence_size_fn size = nullptr;
  meta_sequence_const_element_fn element = nullptr;
  meta_sequence_mutable_element_fn mutable_element = nullptr;
  meta_sequence_clear_fn clear = nullptr;
  meta_sequence_resize_fn resize = nullptr;
  meta_sequence_append_fn append = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return value_type && size != nullptr && element != nullptr;
  }
};

/** @brief 集合容器运行期访问数据。 */
struct meta_set_data {
  type_id value_type{};
  meta_set_size_fn size = nullptr;
  meta_set_visit_fn visit = nullptr;
  meta_set_clear_fn clear = nullptr;
  meta_set_insert_fn insert = nullptr;
  meta_container_iterator_create_fn begin = nullptr;
  meta_container_iterator_create_fn end = nullptr;
  meta_container_iterator_clone_fn iterator_clone = nullptr;
  meta_container_iterator_destroy_fn iterator_destroy = nullptr;
  meta_container_iterator_advance_fn iterator_advance = nullptr;
  meta_container_iterator_equal_fn iterator_equal = nullptr;
  meta_set_iterator_value_fn iterator_value = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return value_type && size != nullptr && visit != nullptr;
  }
};

/** @brief 关联容器运行期访问数据。 */
struct meta_associative_data {
  type_id key_type{};
  type_id value_type{};
  meta_associative_size_fn size = nullptr;
  meta_associative_visit_fn visit = nullptr;
  meta_associative_clear_fn clear = nullptr;
  meta_associative_insert_fn insert = nullptr;
  meta_container_iterator_create_fn begin = nullptr;
  meta_container_iterator_create_fn end = nullptr;
  meta_container_iterator_clone_fn iterator_clone = nullptr;
  meta_container_iterator_destroy_fn iterator_destroy = nullptr;
  meta_container_iterator_advance_fn iterator_advance = nullptr;
  meta_container_iterator_equal_fn iterator_equal = nullptr;
  meta_associative_iterator_key_fn iterator_key = nullptr;
  meta_associative_iterator_value_fn iterator_value = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return key_type && value_type && size != nullptr && visit != nullptr;
  }
};

} // namespace caors

#endif
