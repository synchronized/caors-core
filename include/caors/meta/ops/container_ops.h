// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_OPS_CONTAINER_OPS_H_
#define CAORS_META_OPS_CONTAINER_OPS_H_

#include "caors/memory/resource.h"
#include "caors/meta/type.h"

#include <cstddef>
#include <limits>

namespace caors::meta {

inline constexpr std::size_t dynamic_sequence_size = std::numeric_limits<std::size_t>::max();

/** @brief 容器只读迭代器的类型擦除操作表。 */
struct container_iterator_ops {
  using create_fn = void* (*)(const void*) noexcept;
  using clone_fn = void* (*)(const void*) noexcept;
  using destroy_fn = void (*)(void*) noexcept;
  using advance_fn = void (*)(void*) noexcept;
  using equal_fn = bool (*)(const void*, const void*) noexcept;

  create_fn begin = nullptr;
  create_fn end = nullptr;
  clone_fn clone = nullptr;
  destroy_fn destroy = nullptr;
  advance_fn advance = nullptr;
  equal_fn equal = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return begin != nullptr && end != nullptr && clone != nullptr && destroy != nullptr &&
           advance != nullptr && equal != nullptr;
  }
};

/** @brief 顺序容器运行期访问操作表。 */
struct sequence_ops {
  using get_value_type_fn = type (*)() noexcept;
  using size_fn = std::size_t (*)(const void*) noexcept;
  using const_element_fn = const void* (*)(const void*, std::size_t) noexcept;
  using mutable_element_fn = void* (*)(void*, std::size_t) noexcept;
  using clear_fn = void (*)(void*) noexcept;
  using resize_fn = bool (*)(void*, std::size_t, memory::memory_resource*);
  using append_fn = bool (*)(void*, const void*);

  size_fn size = nullptr;
  const_element_fn element = nullptr;
  mutable_element_fn mutable_element = nullptr;
  clear_fn clear = nullptr;
  resize_fn resize = nullptr;
  append_fn append = nullptr;
  bool can_read = true;
  bool read_in_place = false;
  get_value_type_fn get_value_type = nullptr;
  std::size_t fixed_size = dynamic_sequence_size;

  [[nodiscard]] bool valid() const noexcept {
    return get_value_type != nullptr && size != nullptr && element != nullptr;
  }
};

/** @brief 集合容器运行期访问操作表。 */
struct set_ops {
  using get_value_type_fn = type (*)() noexcept;
  using size_fn = std::size_t (*)(const void*) noexcept;
  using clear_fn = void (*)(void*) noexcept;
  using insert_fn = bool (*)(void*, const void*);
  using iterator_value_fn = const void* (*)(const void*) noexcept;

  size_fn size = nullptr;
  clear_fn clear = nullptr;
  insert_fn insert = nullptr;
  container_iterator_ops iterator{};
  iterator_value_fn iterator_value = nullptr;
  get_value_type_fn get_value_type = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return get_value_type != nullptr && size != nullptr && iterator.valid() &&
           iterator_value != nullptr;
  }
};

/** @brief 关联容器运行期访问操作表。 */
struct associative_ops {
  using get_key_type_fn = type (*)() noexcept;
  using get_value_type_fn = type (*)() noexcept;
  using size_fn = std::size_t (*)(const void*) noexcept;
  using clear_fn = void (*)(void*) noexcept;
  using insert_fn = bool (*)(void*, const void*, const void*);
  using iterator_key_fn = const void* (*)(const void*) noexcept;
  using iterator_value_fn = const void* (*)(const void*) noexcept;

  size_fn size = nullptr;
  clear_fn clear = nullptr;
  insert_fn insert = nullptr;
  container_iterator_ops iterator{};
  iterator_key_fn iterator_key = nullptr;
  iterator_value_fn iterator_value = nullptr;
  get_key_type_fn get_key_type = nullptr;
  get_value_type_fn get_value_type = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return get_key_type != nullptr && get_value_type != nullptr && size != nullptr &&
           iterator.valid() && iterator_key != nullptr && iterator_value != nullptr;
  }
};

} // namespace caors::meta

#endif
