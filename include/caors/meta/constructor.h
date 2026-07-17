// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_CONSTRUCTOR_H_
#define CAORS_META_CONSTRUCTOR_H_

#include "caors/meta/argument.h"
#include "caors/meta/builder.h"
#include "caors/meta/detail/value_ops_model.h"
#include "caors/meta/value_ops.h"

#include <span>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

template <typename Argument> struct constructor_argument_supported {
  using argument_type = std::remove_cv_t<std::remove_reference_t<Argument>>;

  static constexpr bool value =
      std::is_lvalue_reference_v<Argument> || std::is_move_constructible_v<argument_type>;
};

template <typename... Arguments>
inline constexpr bool constructor_arguments_supported_v =
    (constructor_argument_supported<Arguments>::value && ...);

template <typename Argument>
[[nodiscard]] bool constructor_argument_matches(const argument& argument) noexcept {
  using argument_type = std::remove_cv_t<std::remove_reference_t<Argument>>;
  if (!argument.template matches_type<argument_type>()) {
    return false;
  }

  if constexpr (std::is_rvalue_reference_v<Argument>) {
    return argument.rvalue() && argument.writable();
  } else if constexpr (std::is_lvalue_reference_v<Argument> &&
                       !std::is_const_v<std::remove_reference_t<Argument>>) {
    return argument.writable();
  } else {
    return true;
  }
}

template <typename Argument>
using constructor_argument_pass_type = std::conditional_t<
    std::is_rvalue_reference_v<Argument>, std::remove_cv_t<std::remove_reference_t<Argument>>&&,
    std::conditional_t<
        std::is_lvalue_reference_v<Argument> && !std::is_const_v<std::remove_reference_t<Argument>>,
        std::remove_cv_t<std::remove_reference_t<Argument>>&,
        std::conditional_t<std::is_lvalue_reference_v<Argument>,
                           const std::remove_cv_t<std::remove_reference_t<Argument>>&,
                           std::remove_cv_t<std::remove_reference_t<Argument>>>>>;

template <typename Argument>
[[nodiscard]] constructor_argument_pass_type<Argument>
constructor_argument_cast(const argument& argument) noexcept {
  using argument_type = std::remove_cv_t<std::remove_reference_t<Argument>>;
  if constexpr (std::is_rvalue_reference_v<Argument>) {
    auto* result = argument.template try_get_mutable<argument_type>();
    argument.consume();
    return std::move(*result);
  } else if constexpr (std::is_lvalue_reference_v<Argument> &&
                       !std::is_const_v<std::remove_reference_t<Argument>>) {
    return *argument.template try_get_mutable<argument_type>();
  } else if constexpr (!std::is_reference_v<Argument>) {
    if (argument.rvalue()) {
      auto* result = argument.template try_get_mutable<argument_type>();
      argument.consume();
      return std::move(*result);
    }

    return *argument.template try_get<argument_type>();
  } else if constexpr (std::is_lvalue_reference_v<Argument>) {
    return *argument.template try_get<argument_type>();
  } else {
    return *argument.template try_get<argument_type>();
  }
}

template <typename T, typename... Arguments, std::size_t... Indices>
[[nodiscard]] bool constructor_arguments_match(std::span<const argument> arguments,
                                               std::index_sequence<Indices...>) noexcept {
  return arguments.size() == sizeof...(Arguments) &&
         (constructor_argument_matches<Arguments>(arguments[Indices]) && ...);
}

template <typename T, typename... Arguments, std::size_t... Indices>
[[nodiscard]] void* construct_value_with_arguments(memory::memory_resource* resource,
                                                   std::span<const argument> arguments,
                                                   std::index_sequence<Indices...>) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (constructor_arguments_supported_v<Arguments...> &&
                std::is_constructible_v<type, Arguments...>) {
    if (!constructor_arguments_match<type, Arguments...>(arguments,
                                                         std::index_sequence<Indices...>{})) {
      return nullptr;
    }

    return memory::make<type>(resource,
                              constructor_argument_cast<Arguments>(arguments[Indices])...);
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
  if constexpr (constructor_arguments_supported_v<Arguments...> &&
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

namespace caors::meta {

template <typename T>
template <typename... Arguments>
class_builder<T>& class_builder<T>::constructor() {
  if (!type_.registered()) {
    status_ = {error_domain::meta, errc::invalid_type};
    return *this;
  }

  const constructor_ops ops = detail::make_constructor_ops<T, Arguments...>();
  if (!ops.valid()) {
    status_ = {error_domain::meta, errc::invalid_type};
    return *this;
  }

  const constructor_ops* existing = type_.get_raw_ops<constructor_ops>();
  if (existing != nullptr) {
    status_ = existing->construct == ops.construct && existing->destroy == ops.destroy &&
                      existing->argument_count == ops.argument_count
                  ? error{error_domain::meta, errc::duplicate_id}
                  : error{error_domain::meta, errc::invalid_type};
    return *this;
  }

  status_ = detail::register_owned_capability_for_type<constructor_ops, T>(ops);
  refresh();
  return *this;
}

} // namespace caors::meta

#endif
