// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_SEQUENCE_OPS_H_
#define CAORS_META_DETAIL_BUILTIN_SEQUENCE_OPS_H_

#include "caors/memory/resource.h"
#include "caors/meta/detail/builtin/capability.h"
#include "caors/traits/container.h"

#include <cstddef>
#include <limits>
#include <type_traits>

namespace caors::meta::detail {

inline constexpr std::size_t dynamic_sequence_size = std::numeric_limits<std::size_t>::max();

using sequence_size_fn = std::size_t (*)(const void*) noexcept;
using sequence_const_element_fn = const void* (*)(const void*, std::size_t) noexcept;
using sequence_mutable_element_fn = void* (*)(void*, std::size_t) noexcept;
using sequence_clear_fn = void (*)(void*) noexcept;
using sequence_resize_fn = bool (*)(void*, std::size_t, memory::memory_resource*);
using sequence_append_fn = bool (*)(void*, const void*);

/** @brief 顺序容器运行期访问操作表。 */
struct sequence_ops {
  type_id value_type{};
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
    return value_type && size != nullptr && element != nullptr;
  }
};

template <typename T> struct sequence_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using element_type = typename traits::sequence_traits<type>::element_type;

  [[nodiscard]] static std::size_t size(const void* storage) noexcept {
    if (storage == nullptr) {
      return 0;
    }

    return traits::sequence_traits<type>::size(*static_cast<const type*>(storage));
  }

  [[nodiscard]] static const void* element(const void* storage, std::size_t index) noexcept {
    if (storage == nullptr) {
      return nullptr;
    }

    return traits::sequence_traits<type>::element(*static_cast<const type*>(storage), index);
  }

  [[nodiscard]] static void* mutable_element(void* storage, std::size_t index) noexcept {
    if (storage == nullptr) {
      return nullptr;
    }

    return traits::sequence_traits<type>::mutable_element(*static_cast<type*>(storage), index);
  }

  static void clear(void* storage) noexcept {
    if (storage != nullptr) {
      traits::sequence_traits<type>::clear(*static_cast<type*>(storage));
    }
  }

  static bool resize(void* storage, std::size_t size, memory::memory_resource* resource) {
    if (storage == nullptr) {
      return false;
    }

    if constexpr (requires(type& values, std::size_t count,
                           memory::memory_resource* resize_resource) {
                    traits::sequence_traits<type>::resize(values, count, resize_resource);
                  }) {
      return traits::sequence_traits<type>::resize(*static_cast<type*>(storage), size, resource);
    } else {
      (void)resource;
      return traits::sequence_traits<type>::resize(*static_cast<type*>(storage), size);
    }
  }

  static bool append(void* storage, const void* element) {
    if (storage == nullptr || element == nullptr) {
      return false;
    }

    return traits::sequence_traits<type>::append(*static_cast<type*>(storage),
                                                 *static_cast<const element_type*>(element));
  }

  [[nodiscard]] static sequence_ops make() {
    sequence_ops result{
        descriptor_type_id<element_type>(),
        &size,
        &element,
        &mutable_element,
        &clear,
        &resize,
        &append,
    };

    if constexpr (requires { traits::sequence_traits<type>::read_in_place; }) {
      result.read_in_place = traits::sequence_traits<type>::read_in_place;
    }

    if constexpr (requires { traits::sequence_traits<type>::can_read; }) {
      result.can_read = traits::sequence_traits<type>::can_read;
    }

    if constexpr (requires { traits::sequence_traits<type>::fixed_size; }) {
      result.fixed_size = traits::sequence_traits<type>::fixed_size;
    }

    result.value_descriptor = descriptor_ptr_of<element_type>();
    return result;
  }

  inline static const sequence_ops value = make();
};

template <typename T> [[nodiscard]] const sequence_ops* sequence_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::sequence_traits<type>::is_sequence) {
    return sequence_ops_model<type>::value.valid() ? &sequence_ops_model<type>::value : nullptr;
  } else {
    return nullptr;
  }
}

} // namespace caors::meta::detail

#endif
