// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_OPS_H_
#define CAORS_META_TYPE_OPS_H_

#include "caors/core/containers.h"
#include "caors/memory/resource.h"
#include "caors/meta/type.h"
#include "caors/meta/type_id.h"
#include "caors/traits/wrapper.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace caors::meta {

inline constexpr std::size_t dynamic_sequence_size = std::numeric_limits<std::size_t>::max();

/** @brief 枚举类型和整数之间的转换函数表。 */
struct enum_ops {
  using to_i64_fn = std::int64_t (*)(const void*) noexcept;
  using to_u64_fn = std::uint64_t (*)(const void*) noexcept;
  using from_i64_fn = bool (*)(void*, std::int64_t) noexcept;
  using from_u64_fn = bool (*)(void*, std::uint64_t) noexcept;

  to_i64_fn to_i64 = nullptr;
  to_u64_fn to_u64 = nullptr;
  from_i64_fn from_i64 = nullptr;
  from_u64_fn from_u64 = nullptr;
  bool is_signed = false;

  [[nodiscard]] bool valid() const noexcept {
    return to_i64 != nullptr && to_u64 != nullptr && from_i64 != nullptr && from_u64 != nullptr;
  }
};

/** @brief 指针或 wrapper 的内部值访问函数表。 */
struct indirect_ops {
  using get_value_type_fn = type (*)() noexcept;
  using const_value_fn = const void* (*)(const void*) noexcept;
  using mutable_value_fn = void* (*)(void*) noexcept;
  using dynamic_type_fn = type_id (*)(const void*) noexcept;
  using dynamic_value_fn = const void* (*)(const void*) noexcept;
  using mutable_dynamic_value_fn = void* (*)(void*) noexcept;
  using has_value_fn = bool (*)(const void*) noexcept;
  using reset_fn = bool (*)(void*) noexcept;
  using ensure_value_fn = bool (*)(void*, memory::memory_resource*);
  using emplace_value_fn = bool (*)(void*, void*);
  using set_value_fn = bool (*)(void*, void*) noexcept;
  using set_value_with_context_fn = bool (*)(void*, void*,
                                             const traits::wrapper_set_context&) noexcept;

  const_value_fn value = nullptr;
  mutable_value_fn mutable_value = nullptr;
  dynamic_type_fn dynamic_type = nullptr;
  dynamic_value_fn dynamic_value = nullptr;
  mutable_dynamic_value_fn mutable_dynamic_value = nullptr;
  has_value_fn has_value = nullptr;
  reset_fn reset = nullptr;
  ensure_value_fn ensure_value = nullptr;
  emplace_value_fn emplace_value = nullptr;
  set_value_fn set_value = nullptr;
  set_value_with_context_fn set_value_with_context = nullptr;
  get_value_type_fn get_value_type = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return value != nullptr || mutable_value != nullptr || has_value != nullptr ||
           reset != nullptr || ensure_value != nullptr || emplace_value != nullptr ||
           dynamic_type != nullptr || dynamic_value != nullptr ||
           mutable_dynamic_value != nullptr || set_value != nullptr ||
           set_value_with_context != nullptr;
  }
};

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

/** @brief 二元组运行期访问操作表。 */
struct pair_ops {
  using get_key_type_fn = type (*)() noexcept;
  using get_value_type_fn = type (*)() noexcept;
  using const_value_fn = const void* (*)(const void*) noexcept;
  using mutable_value_fn = void* (*)(void*) noexcept;

  const_value_fn first = nullptr;
  mutable_value_fn mutable_first = nullptr;
  const_value_fn second = nullptr;
  mutable_value_fn mutable_second = nullptr;
  get_key_type_fn get_key_type = nullptr;
  get_value_type_fn get_value_type = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return get_key_type != nullptr && get_value_type != nullptr && first != nullptr &&
           second != nullptr;
  }
};

/** @brief tuple-like 单个元素运行期访问操作表。 */
struct tuple_element_ops {
  using get_value_type_fn = type (*)() noexcept;
  using const_element_fn = const void* (*)(const void*) noexcept;
  using mutable_element_fn = void* (*)(void*) noexcept;

  const_element_fn element = nullptr;
  mutable_element_fn mutable_element = nullptr;
  get_value_type_fn get_value_type = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return get_value_type != nullptr && element != nullptr;
  }
};

/** @brief tuple-like 运行期访问操作表。 */
struct tuple_ops {
  vector<tuple_element_ops> elements{};

  [[nodiscard]] bool valid() const noexcept {
    for (const tuple_element_ops& element : elements) {
      if (!element.valid()) {
        return false;
      }
    }

    return true;
  }
};

/** @brief variant-like 单个候选类型运行期访问操作表。 */
struct variant_alternative_ops {
  using get_value_type_fn = type (*)() noexcept;
  using const_alternative_fn = const void* (*)(const void*) noexcept;
  using mutable_alternative_fn = void* (*)(void*) noexcept;
  using emplace_fn = bool (*)(void*, memory::memory_resource*);
  using set_fn = bool (*)(void*, const void*);

  const_alternative_fn alternative = nullptr;
  mutable_alternative_fn mutable_alternative = nullptr;
  emplace_fn emplace = nullptr;
  set_fn set = nullptr;
  get_value_type_fn get_value_type = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return get_value_type != nullptr && alternative != nullptr;
  }
};

/** @brief variant-like 运行期访问操作表。 */
struct variant_ops {
  using index_fn = std::size_t (*)(const void*) noexcept;

  vector<variant_alternative_ops> alternatives{};
  index_fn index = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    if (index == nullptr || alternatives.empty()) {
      return false;
    }

    for (const variant_alternative_ops& alternative : alternatives) {
      if (!alternative.valid()) {
        return false;
      }
    }

    return true;
  }
};

} // namespace caors::meta

#endif
