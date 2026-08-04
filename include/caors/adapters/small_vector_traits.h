// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ADAPTERS_SMALL_VECTOR_TRAITS_H_
#define CAORS_ADAPTERS_SMALL_VECTOR_TRAITS_H_

#include "caors/core/small_vector.h"
#include "caors/traits/container.h"

#include <cstddef>
#include <string_view>
#include <type_traits>

namespace caors::traits {

/** @brief `caors::core::small_vector` 的顺序容器反射适配。 */
template <typename T, std::size_t InlineCapacity>
struct sequence_traits<core::small_vector<T, InlineCapacity>> {
  static constexpr bool is_sequence = true;
  static constexpr std::size_t inline_capacity = InlineCapacity;
  using element_type = std::remove_cv_t<T>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    return "caors.small_vector";
  }

  [[nodiscard]] static std::size_t
  size(const core::small_vector<T, InlineCapacity>& values) noexcept {
    return values.size();
  }

  [[nodiscard]] static const element_type*
  element(const core::small_vector<T, InlineCapacity>& values, std::size_t index) noexcept {
    return index < values.size() ? &values[index] : nullptr;
  }

  [[nodiscard]] static element_type* mutable_element(core::small_vector<T, InlineCapacity>& values,
                                                     std::size_t index) noexcept {
    return index < values.size() ? &values[index] : nullptr;
  }

  static void clear(core::small_vector<T, InlineCapacity>& values) noexcept { values.clear(); }

  [[nodiscard]] static bool resize(core::small_vector<T, InlineCapacity>& values,
                                   std::size_t size) {
    if constexpr (std::is_default_constructible_v<T>) {
      values.resize(size);
      return true;
    } else {
      return false;
    }
  }

  [[nodiscard]] static bool resize(core::small_vector<T, InlineCapacity>& values, std::size_t size,
                                   memory::memory_resource*) {
    return resize(values, size);
  }

  [[nodiscard]] static bool append(core::small_vector<T, InlineCapacity>& values,
                                   const element_type& value) {
    if constexpr (std::is_copy_constructible_v<T>) {
      values.push_back(value);
      return true;
    } else {
      return false;
    }
  }
};

} // namespace caors::traits

#endif
