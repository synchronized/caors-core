// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_OPS_H_
#define CAORS_META_TYPE_OPS_H_

#include "caors/core/containers.h"
#include "caors/memory/resource.h"
#include "caors/meta/type_id.h"
#include "caors/traits/wrapper.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace caors::meta {

struct type_descriptor;

inline constexpr std::size_t dynamic_sequence_size = std::numeric_limits<std::size_t>::max();

using enum_to_i64_fn = std::int64_t (*)(const void*) noexcept;
using enum_to_u64_fn = std::uint64_t (*)(const void*) noexcept;
using enum_from_i64_fn = bool (*)(void*, std::int64_t) noexcept;
using enum_from_u64_fn = bool (*)(void*, std::uint64_t) noexcept;

/** @brief 枚举类型和整数之间的转换函数表。 */
struct enum_ops {
  enum_to_i64_fn to_i64 = nullptr;
  enum_to_u64_fn to_u64 = nullptr;
  enum_from_i64_fn from_i64 = nullptr;
  enum_from_u64_fn from_u64 = nullptr;
  bool is_signed = false;

  [[nodiscard]] bool valid() const noexcept {
    return to_i64 != nullptr && to_u64 != nullptr && from_i64 != nullptr && from_u64 != nullptr;
  }
};

using indirect_const_value_fn = const void* (*)(const void*) noexcept;
using indirect_mutable_value_fn = void* (*)(void*) noexcept;
using indirect_dynamic_type_fn = type_id (*)(const void*) noexcept;
using indirect_dynamic_value_fn = const void* (*)(const void*) noexcept;
using indirect_mutable_dynamic_value_fn = void* (*)(void*) noexcept;
using indirect_has_value_fn = bool (*)(const void*) noexcept;
using indirect_reset_fn = bool (*)(void*) noexcept;
using indirect_ensure_value_fn = bool (*)(void*, memory::memory_resource*);
using indirect_set_value_fn = bool (*)(void*, void*) noexcept;
using indirect_set_value_with_context_fn = bool (*)(void*, void*,
                                                    const traits::wrapper_set_context&) noexcept;

/** @brief 指针或 wrapper 的内部值访问函数表。 */
struct indirect_ops {
  indirect_const_value_fn value = nullptr;
  indirect_mutable_value_fn mutable_value = nullptr;
  indirect_dynamic_type_fn dynamic_type = nullptr;
  indirect_dynamic_value_fn dynamic_value = nullptr;
  indirect_mutable_dynamic_value_fn mutable_dynamic_value = nullptr;
  indirect_has_value_fn has_value = nullptr;
  indirect_reset_fn reset = nullptr;
  indirect_ensure_value_fn ensure_value = nullptr;
  indirect_set_value_fn set_value = nullptr;
  indirect_set_value_with_context_fn set_value_with_context = nullptr;
  const type_descriptor* value_descriptor = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return value != nullptr || mutable_value != nullptr || has_value != nullptr ||
           reset != nullptr || ensure_value != nullptr || dynamic_type != nullptr ||
           dynamic_value != nullptr || mutable_dynamic_value != nullptr || set_value != nullptr ||
           set_value_with_context != nullptr;
  }
};

using container_iterator_create_fn = void* (*)(const void*) noexcept;
using container_iterator_clone_fn = void* (*)(const void*) noexcept;
using container_iterator_destroy_fn = void (*)(void*) noexcept;
using container_iterator_advance_fn = void (*)(void*) noexcept;
using container_iterator_equal_fn = bool (*)(const void*, const void*) noexcept;

/** @brief 容器只读迭代器的类型擦除操作表。 */
struct container_iterator_ops {
  container_iterator_create_fn begin = nullptr;
  container_iterator_create_fn end = nullptr;
  container_iterator_clone_fn clone = nullptr;
  container_iterator_destroy_fn destroy = nullptr;
  container_iterator_advance_fn advance = nullptr;
  container_iterator_equal_fn equal = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return begin != nullptr && end != nullptr && clone != nullptr && destroy != nullptr &&
           advance != nullptr && equal != nullptr;
  }
};

using sequence_size_fn = std::size_t (*)(const void*) noexcept;
using sequence_const_element_fn = const void* (*)(const void*, std::size_t) noexcept;
using sequence_mutable_element_fn = void* (*)(void*, std::size_t) noexcept;
using sequence_clear_fn = void (*)(void*) noexcept;
using sequence_resize_fn = bool (*)(void*, std::size_t, memory::memory_resource*);
using sequence_append_fn = bool (*)(void*, const void*);

/** @brief 顺序容器运行期访问操作表。 */
struct sequence_ops {
  sequence_size_fn size = nullptr;
  sequence_const_element_fn element = nullptr;
  sequence_mutable_element_fn mutable_element = nullptr;
  sequence_clear_fn clear = nullptr;
  sequence_resize_fn resize = nullptr;
  sequence_append_fn append = nullptr;
  bool can_read = true;
  bool read_in_place = false;
  const type_descriptor* value_descriptor = nullptr;
  std::size_t fixed_size = dynamic_sequence_size;

  [[nodiscard]] bool valid() const noexcept {
    return value_descriptor != nullptr && size != nullptr && element != nullptr;
  }
};

using set_size_fn = std::size_t (*)(const void*) noexcept;
using set_clear_fn = void (*)(void*) noexcept;
using set_insert_fn = bool (*)(void*, const void*);
using set_iterator_value_fn = const void* (*)(const void*) noexcept;

/** @brief 集合容器运行期访问操作表。 */
struct set_ops {
  set_size_fn size = nullptr;
  set_clear_fn clear = nullptr;
  set_insert_fn insert = nullptr;
  container_iterator_ops iterator{};
  set_iterator_value_fn iterator_value = nullptr;
  const type_descriptor* value_descriptor = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return value_descriptor != nullptr && size != nullptr && iterator.valid() &&
           iterator_value != nullptr;
  }
};

using associative_size_fn = std::size_t (*)(const void*) noexcept;
using associative_clear_fn = void (*)(void*) noexcept;
using associative_insert_fn = bool (*)(void*, const void*, const void*);
using associative_iterator_key_fn = const void* (*)(const void*) noexcept;
using associative_iterator_value_fn = const void* (*)(const void*) noexcept;

/** @brief 关联容器运行期访问操作表。 */
struct associative_ops {
  associative_size_fn size = nullptr;
  associative_clear_fn clear = nullptr;
  associative_insert_fn insert = nullptr;
  container_iterator_ops iterator{};
  associative_iterator_key_fn iterator_key = nullptr;
  associative_iterator_value_fn iterator_value = nullptr;
  const type_descriptor* key_descriptor = nullptr;
  const type_descriptor* value_descriptor = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return key_descriptor != nullptr && value_descriptor != nullptr && size != nullptr &&
           iterator.valid() && iterator_key != nullptr && iterator_value != nullptr;
  }
};

using pair_const_value_fn = const void* (*)(const void*) noexcept;
using pair_mutable_value_fn = void* (*)(void*) noexcept;

/** @brief 二元组运行期访问操作表。 */
struct pair_ops {
  pair_const_value_fn first = nullptr;
  pair_mutable_value_fn mutable_first = nullptr;
  pair_const_value_fn second = nullptr;
  pair_mutable_value_fn mutable_second = nullptr;
  const type_descriptor* key_descriptor = nullptr;
  const type_descriptor* value_descriptor = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return key_descriptor != nullptr && value_descriptor != nullptr && first != nullptr &&
           second != nullptr;
  }
};

using tuple_const_element_fn = const void* (*)(const void*) noexcept;
using tuple_mutable_element_fn = void* (*)(void*) noexcept;

/** @brief tuple-like 单个元素运行期访问操作表。 */
struct tuple_element_ops {
  tuple_const_element_fn element = nullptr;
  tuple_mutable_element_fn mutable_element = nullptr;
  const type_descriptor* value_descriptor = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return value_descriptor != nullptr && element != nullptr;
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

using variant_index_fn = std::size_t (*)(const void*) noexcept;
using variant_const_alternative_fn = const void* (*)(const void*) noexcept;
using variant_mutable_alternative_fn = void* (*)(void*) noexcept;
using variant_emplace_fn = bool (*)(void*, memory::memory_resource*);

/** @brief variant-like 单个候选类型运行期访问操作表。 */
struct variant_alternative_ops {
  variant_const_alternative_fn alternative = nullptr;
  variant_mutable_alternative_fn mutable_alternative = nullptr;
  variant_emplace_fn emplace = nullptr;
  const type_descriptor* value_descriptor = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return value_descriptor != nullptr && alternative != nullptr;
  }
};

/** @brief variant-like 运行期访问操作表。 */
struct variant_ops {
  vector<variant_alternative_ops> alternatives{};
  variant_index_fn index = nullptr;

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
