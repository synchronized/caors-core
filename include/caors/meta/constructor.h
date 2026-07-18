// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_CONSTRUCTOR_H_
#define CAORS_META_CONSTRUCTOR_H_

#include "caors/meta/detail/argument_access.h"
#include "caors/meta/detail/value_ops_model.h"
#include "caors/meta/value_ops.h"

#include <span>
#include <type_traits>

namespace caors::meta::detail {

template <typename T, typename... Arguments, std::size_t... Indices>
[[nodiscard]] bool constructor_arguments_match(std::span<const argument> arguments,
                                               std::index_sequence<Indices...>) noexcept {
  return arguments.size() == sizeof...(Arguments) &&
         (argument_matches<Arguments>(arguments[Indices]) && ...);
}

template <typename T, typename... Arguments, std::size_t... Indices>
[[nodiscard]] void* construct_value_with_arguments(memory::memory_resource* resource,
                                                   std::span<const argument> arguments,
                                                   std::index_sequence<Indices...>) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (call_arguments_supported_v<Arguments...> &&
                std::is_constructible_v<type, Arguments...>) {
    if (!constructor_arguments_match<type, Arguments...>(arguments,
                                                         std::index_sequence<Indices...>{})) {
      return nullptr;
    }

    return memory::make<type>(resource, argument_value<Arguments>(arguments[Indices])...);
  } else {
    return nullptr;
  }
}

template <typename T, typename... Arguments>
[[nodiscard]] void* construct_value_with_arguments(memory::memory_resource* resource,
                                                   std::span<const argument> arguments) {
  return construct_value_with_arguments<T, Arguments...>(resource, arguments,
                                                         std::index_sequence_for<Arguments...>{});
}

template <typename T, typename... Arguments>
[[nodiscard]] constructor_ops make_constructor_ops() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using destroy_type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (call_arguments_supported_v<Arguments...> &&
                std::is_constructible_v<type, Arguments...> && std::is_destructible_v<type>) {
    return constructor_ops{
        &construct_value_with_arguments<type, Arguments...>,
        &resource_destroy_value<destroy_type>,
        sizeof...(Arguments),
    };
  } else {
    return {};
  }
}

} // namespace caors::meta::detail

#endif
