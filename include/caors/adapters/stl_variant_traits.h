// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ADAPTERS_STL_VARIANT_TRAITS_H_
#define CAORS_ADAPTERS_STL_VARIANT_TRAITS_H_

#include "caors/memory/resource.h"
#include "caors/traits/container.h"

#include <cstddef>
#include <string_view>
#include <type_traits>
#include <variant>

namespace caors::traits {
/** @brief `std::variant` 的 variant-like 反射适配。 */
template <typename... Types> struct variant_traits<std::variant<Types...>> {
  static constexpr bool is_variant = true;
  static constexpr std::size_t size = sizeof...(Types);

  template <std::size_t Index>
  using alternative_type =
      std::remove_cv_t<std::variant_alternative_t<Index, std::variant<Types...>>>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    return "std.variant";
  }

  [[nodiscard]] static std::size_t index(const std::variant<Types...>& value) noexcept {
    return value.index();
  }

  template <std::size_t Index>
  [[nodiscard]] static const alternative_type<Index>*
  alternative(const std::variant<Types...>& value) noexcept {
    return std::get_if<Index>(&value);
  }

  template <std::size_t Index>
  [[nodiscard]] static alternative_type<Index>*
  mutable_alternative(std::variant<Types...>& value) noexcept {
    using raw_alternative_type = std::variant_alternative_t<Index, std::variant<Types...>>;
    if constexpr (std::is_const_v<raw_alternative_type>) {
      (void)value;
      return nullptr;
    } else {
      return std::get_if<Index>(&value);
    }
  }

  template <std::size_t Index> [[nodiscard]] static bool emplace(std::variant<Types...>& value) {
    using raw_alternative_type = std::variant_alternative_t<Index, std::variant<Types...>>;
    if constexpr (std::is_default_constructible_v<raw_alternative_type>) {
      value.template emplace<Index>();
      return true;
    } else {
      (void)value;
      return false;
    }
  }

  template <std::size_t Index>
  [[nodiscard]] static bool emplace(std::variant<Types...>& value,
                                    memory::memory_resource* resource) {
    using raw_alternative_type = std::variant_alternative_t<Index, std::variant<Types...>>;
    if constexpr (std::is_default_constructible_v<raw_alternative_type>) {
      if constexpr (std::uses_allocator_v<raw_alternative_type,
                                          memory::allocator<raw_alternative_type>>) {
        if (resource == nullptr) {
          value.template emplace<Index>();
        } else {
          value.template emplace<Index>(memory::allocator<raw_alternative_type>{resource});
        }
      } else {
        (void)resource;
        value.template emplace<Index>();
      }
      return true;
    } else if constexpr (std::uses_allocator_v<raw_alternative_type,
                                               memory::allocator<raw_alternative_type>>) {
      if (resource != nullptr) {
        value.template emplace<Index>(memory::allocator<raw_alternative_type>{resource});
        return true;
      } else {
        (void)value;
        return false;
      }
    } else {
      (void)value;
      (void)resource;
      return false;
    }
  }
};

} // namespace caors::traits

#endif
