// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_ARGUMENT_ACCESS_H_
#define CAORS_META_DETAIL_ARGUMENT_ACCESS_H_

#include "caors/core/containers.h"
#include "caors/meta/argument.h"

#include <array>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

template <typename T> using argument_value_type = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename Argument> struct call_argument_supported {
  static constexpr bool value = std::is_lvalue_reference_v<Argument> ||
                                std::is_move_constructible_v<argument_value_type<Argument>>;
};

template <typename... Arguments>
inline constexpr bool call_arguments_supported_v =
    (call_argument_supported<Arguments>::value && ...);

template <typename Parameter> [[nodiscard]] bool argument_matches(const argument& value) noexcept {
  using value_type = argument_value_type<Parameter>;
  if (!value.template matches_type<value_type>()) {
    return false;
  }

  if constexpr (std::is_rvalue_reference_v<Parameter>) {
    return value.rvalue() && value.writable();
  } else if constexpr (std::is_lvalue_reference_v<Parameter> &&
                       !std::is_const_v<std::remove_reference_t<Parameter>>) {
    return value.writable();
  } else {
    return true;
  }
}

enum class argument_match_rank : unsigned char {
  none,
  conversion,
  exact,
};

template <typename Target, typename Source>
[[nodiscard]] consteval bool safe_numeric_conversion() noexcept {
  using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
  using source_type = std::remove_cv_t<std::remove_reference_t<Source>>;

  if constexpr (!std::is_arithmetic_v<target_type> || !std::is_arithmetic_v<source_type> ||
                std::is_same_v<target_type, bool> || std::is_same_v<source_type, bool>) {
    return false;
  } else if constexpr (std::is_same_v<target_type, source_type>) {
    return true;
  } else if constexpr (std::is_integral_v<source_type> && std::is_integral_v<target_type>) {
    if constexpr (std::is_signed_v<source_type> == std::is_signed_v<target_type>) {
      return std::numeric_limits<target_type>::digits >= std::numeric_limits<source_type>::digits;
    } else if constexpr (!std::is_signed_v<source_type> && std::is_signed_v<target_type>) {
      return std::numeric_limits<target_type>::digits > std::numeric_limits<source_type>::digits;
    } else {
      return false;
    }
  } else if constexpr (std::is_floating_point_v<source_type> &&
                       std::is_floating_point_v<target_type>) {
    return std::numeric_limits<target_type>::digits >= std::numeric_limits<source_type>::digits &&
           std::numeric_limits<target_type>::max_exponent >=
               std::numeric_limits<source_type>::max_exponent;
  } else if constexpr (std::is_integral_v<source_type> && std::is_floating_point_v<target_type>) {
    return std::numeric_limits<target_type>::digits >= std::numeric_limits<source_type>::digits;
  } else {
    return false;
  }
}

template <typename Target, typename Source>
[[nodiscard]] std::optional<argument> try_convert_numeric_from(const argument& value) {
  using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
  using source_type = std::remove_cv_t<std::remove_reference_t<Source>>;

  if constexpr (std::is_same_v<target_type, source_type>) {
    return std::nullopt;
  } else if constexpr (safe_numeric_conversion<target_type, source_type>()) {
    const source_type* source = value.template try_get<source_type>();
    if (source == nullptr) {
      return std::nullopt;
    }

    const target_type converted = static_cast<target_type>(*source);
    return argument::move(any::from(converted));
  } else {
    return std::nullopt;
  }
}

template <typename Target>
[[nodiscard]] std::optional<argument> try_convert_numeric_argument(const argument& value) {
  if (auto converted = try_convert_numeric_from<Target, char>(value)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, signed char>(value)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, unsigned char>(value)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, short>(value)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, unsigned short>(value)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, int>(value)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, unsigned int>(value)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, long>(value)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, unsigned long>(value)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, long long>(value)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, unsigned long long>(value)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, float>(value)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, double>(value)) {
    return converted;
  }

  return std::nullopt;
}

template <typename Target>
[[nodiscard]] bool custom_argument_conversion_available(const argument& value) noexcept {
  using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
  if (!value || !value.get_type()) {
    return false;
  }

  const argument_conversion_ops* ops =
      value.get_type().template get_capability<argument_conversion_ops>();
  const meta::type target = meta::resolve<target_type>();
  return ops != nullptr && ops->valid() && target && ops->can_convert(target);
}

template <typename Target>
[[nodiscard]] std::optional<argument> try_convert_custom_argument(const argument& value) {
  using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
  if (!value || !value.get_type()) {
    return std::nullopt;
  }

  const argument_conversion_ops* ops =
      value.get_type().template get_capability<argument_conversion_ops>();
  const meta::type target = meta::resolve<target_type>();
  if (ops == nullptr || !ops->valid() || !target || !ops->can_convert(target)) {
    return std::nullopt;
  }

  argument converted = ops->convert(value, target);
  if (!argument_matches<target_type>(converted)) {
    return std::nullopt;
  }

  return converted;
}

template <typename Parameter>
[[nodiscard]] argument_match_rank argument_rank(const argument& value,
                                                argument_match_options options) noexcept {
  if (argument_matches<Parameter>(value)) {
    return argument_match_rank::exact;
  }

  using value_type = argument_value_type<Parameter>;
  if constexpr (std::is_arithmetic_v<value_type> && !std::is_reference_v<Parameter>) {
    if (options.conversion_policy == argument_conversion_policy::numeric_safe &&
        try_convert_numeric_argument<value_type>(value).has_value()) {
      return argument_match_rank::conversion;
    }
  }

  if constexpr (!std::is_reference_v<Parameter>) {
    if (options.allow_custom_conversions &&
        custom_argument_conversion_available<value_type>(value)) {
      return argument_match_rank::conversion;
    }
  }

  return argument_match_rank::none;
}

template <typename Parameter>
[[nodiscard]] argument prepare_argument(const argument& value, argument_match_options options) {
  if (argument_matches<Parameter>(value)) {
    return value;
  }

  using value_type = argument_value_type<Parameter>;
  if constexpr (std::is_arithmetic_v<value_type> && !std::is_reference_v<Parameter>) {
    if (options.conversion_policy == argument_conversion_policy::numeric_safe) {
      if (auto converted = try_convert_numeric_argument<value_type>(value)) {
        return std::move(*converted);
      }
    }
  }

  if constexpr (!std::is_reference_v<Parameter>) {
    if (options.allow_custom_conversions) {
      if (auto converted = try_convert_custom_argument<value_type>(value)) {
        return std::move(*converted);
      }
    }
  }

  return {};
}

template <typename Parameter>
using argument_pass_type =
    std::conditional_t<std::is_rvalue_reference_v<Parameter>, argument_value_type<Parameter>&&,
                       std::conditional_t<std::is_lvalue_reference_v<Parameter> &&
                                              !std::is_const_v<std::remove_reference_t<Parameter>>,
                                          argument_value_type<Parameter>&,
                                          std::conditional_t<std::is_lvalue_reference_v<Parameter>,
                                                             const argument_value_type<Parameter>&,
                                                             argument_value_type<Parameter>>>>;

template <typename Parameter>
argument_pass_type<Parameter> argument_value(const argument& value) noexcept {
  using value_type = argument_value_type<Parameter>;
  if constexpr (std::is_rvalue_reference_v<Parameter>) {
    auto* result = value.template try_get_mutable<value_type>();
    value.consume();
    return std::move(*result);
  } else if constexpr (std::is_lvalue_reference_v<Parameter> &&
                       !std::is_const_v<std::remove_reference_t<Parameter>>) {
    return *value.template try_get_mutable<value_type>();
  } else if constexpr (!std::is_reference_v<Parameter>) {
    if (value.rvalue()) {
      auto* result = value.template try_get_mutable<value_type>();
      value.consume();
      return std::move(*result);
    }

    return *value.template try_get<value_type>();
  } else {
    return *value.template try_get<value_type>();
  }
}

template <typename... Arguments>
[[nodiscard]] bool arguments_match(std::span<const argument> arguments,
                                   argument_match_options options = {}) noexcept {
  if (arguments.size() != sizeof...(Arguments)) {
    return false;
  }

  std::size_t index = 0;
  return ((argument_rank<Arguments>(arguments[index++], options) != argument_match_rank::none) &&
          ...);
}

template <typename... Arguments>
[[nodiscard]] argument_match_rank arguments_rank(std::span<const argument> arguments,
                                                 argument_match_options options = {}) noexcept {
  if (arguments.size() != sizeof...(Arguments)) {
    return argument_match_rank::none;
  }

  argument_match_rank result = argument_match_rank::exact;
  std::size_t index = 0;
  (([&] {
     const argument_match_rank rank = argument_rank<Arguments>(arguments[index++], options);
     if (rank == argument_match_rank::none) {
       result = argument_match_rank::none;
     } else if (rank == argument_match_rank::conversion && result == argument_match_rank::exact) {
       result = argument_match_rank::conversion;
     }
   }()),
   ...);

  return result;
}

template <typename... Arguments>
[[nodiscard]] vector<argument> prepare_arguments(std::span<const argument> arguments,
                                                 argument_match_options options = {}) {
  if (arguments.size() != sizeof...(Arguments)) {
    return {};
  }

  vector<argument> result{};
  result.reserve(sizeof...(Arguments));
  std::size_t index = 0;
  bool succeeded = true;
  (([&] {
     argument prepared = prepare_argument<Arguments>(arguments[index++], options);
     if (!prepared) {
       succeeded = false;
       return;
     }

     result.push_back(std::move(prepared));
   }()),
   ...);

  return succeeded ? result : vector<argument>{};
}

} // namespace caors::meta::detail

#endif
