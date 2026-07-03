// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ADAPTERS_STL_PAIR_TUPLE_TRAITS_H_
#define CAORS_ADAPTERS_STL_PAIR_TUPLE_TRAITS_H_

#include "caors/traits/container.h"

#include <cstddef>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace caors::traits {
/** @brief `std::pair` 的二元组反射适配。 */
template <typename First, typename Second> struct pair_traits<std::pair<First, Second>> {
  static constexpr bool is_pair = true;
  using first_type = std::remove_cv_t<First>;
  using second_type = std::remove_cv_t<Second>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept { return "std.pair"; }

  [[nodiscard]] static const first_type* first(const std::pair<First, Second>& values) noexcept {
    return &values.first;
  }

  [[nodiscard]] static first_type* mutable_first(std::pair<First, Second>& values) noexcept {
    if constexpr (std::is_const_v<First>) {
      (void)values;
      return nullptr;
    } else {
      return &values.first;
    }
  }

  [[nodiscard]] static const second_type* second(const std::pair<First, Second>& values) noexcept {
    return &values.second;
  }

  [[nodiscard]] static second_type* mutable_second(std::pair<First, Second>& values) noexcept {
    if constexpr (std::is_const_v<Second>) {
      (void)values;
      return nullptr;
    } else {
      return &values.second;
    }
  }
};

/** @brief `std::tuple` 的 tuple-like 反射适配。 */
template <typename... Types> struct tuple_traits<std::tuple<Types...>> {
  static constexpr bool is_tuple = true;
  static constexpr std::size_t size = sizeof...(Types);

  template <std::size_t Index>
  using element_type = std::remove_cv_t<std::tuple_element_t<Index, std::tuple<Types...>>>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    return "std.tuple";
  }

  template <std::size_t Index>
  [[nodiscard]] static const element_type<Index>*
  element(const std::tuple<Types...>& values) noexcept {
    return &std::get<Index>(values);
  }

  template <std::size_t Index>
  [[nodiscard]] static element_type<Index>* mutable_element(std::tuple<Types...>& values) noexcept {
    using raw_element_type = std::tuple_element_t<Index, std::tuple<Types...>>;
    if constexpr (std::is_const_v<raw_element_type>) {
      (void)values;
      return nullptr;
    } else {
      return &std::get<Index>(values);
    }
  }
};

} // namespace caors::traits

#endif
