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

using meta::dynamic_sequence_size;
using meta::sequence_ops;

template <typename T> struct sequence_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using element_type = typename traits::sequence_traits<type>::element_type;

  [[nodiscard]] static meta::type get_value_type() noexcept {
    return meta::make_type_view(descriptor_type_id<element_type>(),
                                descriptor_ptr_of<element_type>());
  }

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
        &size, &element, &mutable_element, &clear, &resize, &append,
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

    result.get_value_type = &get_value_type;
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
