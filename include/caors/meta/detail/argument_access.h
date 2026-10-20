// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_ARGUMENT_ACCESS_H_
#define CAORS_META_DETAIL_ARGUMENT_ACCESS_H_

#include "caors/core/containers.h"
#include "caors/core/guid.h"
#include "caors/core/type_guid.h"
#include "caors/meta/argument.h"
#include "caors/meta/value/capabilities/enum_capability.h"

#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <locale>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
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
[[nodiscard]] std::optional<argument> try_convert_numeric_from(const argument& value,
                                                               memory::memory_resource* resource) {
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
    return argument::move(any::from(converted, resource));
  } else {
    return std::nullopt;
  }
}

template <typename Target>
[[nodiscard]] std::optional<argument>
try_convert_numeric_argument(const argument& value, memory::memory_resource* resource) {
  if (auto converted = try_convert_numeric_from<Target, char>(value, resource)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, signed char>(value, resource)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, unsigned char>(value, resource)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, short>(value, resource)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, unsigned short>(value, resource)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, int>(value, resource)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, unsigned int>(value, resource)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, long>(value, resource)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, unsigned long>(value, resource)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, long long>(value, resource)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, unsigned long long>(value, resource)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, float>(value, resource)) {
    return converted;
  }
  if (auto converted = try_convert_numeric_from<Target, double>(value, resource)) {
    return converted;
  }

  return std::nullopt;
}

template <typename Target>
[[nodiscard]] std::optional<argument> try_convert_numeric_argument(const argument& value) {
  return try_convert_numeric_argument<Target>(value, nullptr);
}

[[nodiscard]] inline std::optional<std::string_view>
try_get_argument_string_view(const argument& value) noexcept {
  if (const auto* text = value.try_get<std::string_view>(); text != nullptr) {
    return *text;
  }
  if (const auto* text = value.try_get<core::string>(); text != nullptr) {
    return std::string_view{text->data(), text->size()};
  }
  if (const auto* text = value.try_get<core::string_name>(); text != nullptr) {
    return text->view();
  }

  return std::nullopt;
}

[[nodiscard]] inline bool parse_bool_argument(std::string_view text, bool& output) noexcept {
  if (text == "true" || text == "1") {
    output = true;
    return true;
  }
  if (text == "false" || text == "0") {
    output = false;
    return true;
  }

  return false;
}

template <typename Target>
[[nodiscard]] bool parse_integral_argument(std::string_view text, Target& output) noexcept {
  using value_type = std::remove_cv_t<std::remove_reference_t<Target>>;

  if constexpr (std::is_same_v<value_type, bool>) {
    return parse_bool_argument(text, output);
  } else if constexpr (std::is_signed_v<value_type>) {
    long long parsed = 0;
    const auto* first = text.data();
    const auto* last = text.data() + text.size();
    const auto [position, error] = std::from_chars(first, last, parsed, 10);
    if (error != std::errc{} || position != last ||
        parsed < static_cast<long long>(std::numeric_limits<value_type>::min()) ||
        parsed > static_cast<long long>(std::numeric_limits<value_type>::max())) {
      return false;
    }

    output = static_cast<value_type>(parsed);
    return true;
  } else {
    unsigned long long parsed = 0;
    const auto* first = text.data();
    const auto* last = text.data() + text.size();
    const auto [position, error] = std::from_chars(first, last, parsed, 10);
    if (error != std::errc{} || position != last ||
        parsed > static_cast<unsigned long long>(std::numeric_limits<value_type>::max())) {
      return false;
    }

    output = static_cast<value_type>(parsed);
    return true;
  }
}

template <typename Float>
inline constexpr bool floating_from_chars_supported = requires(
    const char* first, const char* last, Float& value) { std::from_chars(first, last, value); };

template <typename Target>
[[nodiscard]] bool parse_floating_argument(std::string_view text, Target& output) noexcept {
  // 使用依赖类型探测，避免某些 libc++ 的 long double 重载被显式删除。
  using parse_type =
      std::conditional_t<floating_from_chars_supported<long double>, long double, Target>;
  parse_type parsed = 0;
  if constexpr (requires { std::from_chars(text.data(), text.data() + text.size(), parsed); }) {
    const auto [position, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (error != std::errc{} || position != text.data() + text.size())
      return false;
  } else {
    if (text.empty() || text.front() == '+')
      return false;
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      std::istringstream stream{std::string{text}};
      stream.imbue(std::locale::classic());
      stream >> std::noskipws >> parsed;
      if (stream.fail() || !stream.eof())
        return false;
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      return false;
    }
#endif
  }
  if (!std::isfinite(parsed) ||
      parsed < -static_cast<long double>(std::numeric_limits<Target>::max()) ||
      parsed > static_cast<long double>(std::numeric_limits<Target>::max())) {
    return false;
  }

  output = static_cast<Target>(parsed);
  return true;
}

template <typename Target>
[[nodiscard]] std::optional<argument> try_parse_string_argument(const argument& value,
                                                                memory::memory_resource* resource) {
  using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
  const std::optional<std::string_view> text = try_get_argument_string_view(value);
  if (!text.has_value()) {
    return std::nullopt;
  }

  if constexpr (std::is_integral_v<target_type>) {
    target_type parsed{};
    if (!parse_integral_argument(*text, parsed)) {
      return std::nullopt;
    }

    return argument::move(any::from(parsed, resource));
  } else if constexpr (std::is_floating_point_v<target_type>) {
    target_type parsed{};
    if (!parse_floating_argument(*text, parsed)) {
      return std::nullopt;
    }

    return argument::move(any::from(parsed, resource));
  } else if constexpr (std::is_same_v<target_type, core::string_name>) {
    return argument::move(any::from(core::string_name{*text}, resource));
  } else if constexpr (std::is_same_v<target_type, core::string>) {
    return argument::move(any::from(core::string{*text}, resource));
  } else if constexpr (std::is_same_v<target_type, std::string>) {
    return argument::move(any::from(std::string{*text}, resource));
  } else if constexpr (std::is_same_v<target_type, core::guid>) {
    const std::optional<core::guid> parsed = core::guid::parse(*text);
    return parsed.has_value()
               ? std::optional<argument>{argument::move(any::from(*parsed, resource))}
               : std::nullopt;
  } else if constexpr (std::is_same_v<target_type, core::type_guid>) {
    const std::optional<core::guid> parsed = core::guid::parse(*text);
    return parsed.has_value() ? std::optional<argument>{argument::move(
                                    any::from(core::type_guid{*parsed}, resource))}
                              : std::nullopt;
  } else if constexpr (std::is_enum_v<target_type>) {
    const meta::type enum_type = meta::resolve<target_type>();
    const meta::enum_capability values = enum_type.get_capability<meta::enum_capability>();
    if (const meta::enum_value named = values.find_value(*text)) {
      const std::optional<target_type> parsed = named.template get<target_type>();
      return parsed.has_value()
                 ? std::optional<argument>{argument::move(any::from(*parsed, resource))}
                 : std::nullopt;
    }

    using underlying_type = std::underlying_type_t<target_type>;
    underlying_type parsed{};
    if (!parse_integral_argument(*text, parsed)) {
      return std::nullopt;
    }

    return argument::move(any::from(static_cast<target_type>(parsed), resource));
  } else {
    return std::nullopt;
  }
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

template <typename Target>
[[nodiscard]] std::optional<argument> try_convert_argument(const argument& value,
                                                           argument_match_options options) {
  using value_type = argument_value_type<Target>;
  if constexpr (std::is_reference_v<Target>) {
    return std::nullopt;
  } else {
    if constexpr (std::is_arithmetic_v<value_type>) {
      if (options.conversion_policy == argument_conversion_policy::numeric_safe) {
        if (auto converted =
                try_convert_numeric_argument<value_type>(value, options.scratch_resource)) {
          return converted;
        }
      }
    }

    if (options.conversion_policy == argument_conversion_policy::string_parse) {
      if (auto converted = try_parse_string_argument<value_type>(value, options.scratch_resource)) {
        return converted;
      }
    }

    if (options.allow_custom_conversions) {
      if (auto converted = try_convert_custom_argument<value_type>(value)) {
        return converted;
      }
    }

    return std::nullopt;
  }
}

template <typename Target>
[[nodiscard]] std::optional<argument> try_convert_argument(const argument& value,
                                                           argument_conversion_policy policy,
                                                           bool allow_custom_conversions) {
  argument_match_options options{};
  options.conversion_policy = policy;
  options.allow_custom_conversions = allow_custom_conversions;
  return try_convert_argument<Target>(value, options);
}

template <typename Parameter>
[[nodiscard]] argument_match_rank argument_rank(const argument& value,
                                                argument_match_options options) noexcept {
  if (argument_matches<Parameter>(value)) {
    return argument_match_rank::exact;
  }

  if constexpr (!std::is_reference_v<Parameter>) {
    if (try_convert_argument<Parameter>(value, options).has_value()) {
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

  if constexpr (!std::is_reference_v<Parameter>) {
    if (auto converted = try_convert_argument<Parameter>(value, options)) {
      return std::move(*converted);
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

// 空参数包展开后不会访问匹配选项或游标。
template <typename... Arguments>
[[nodiscard]] bool arguments_match(std::span<const argument> arguments,
                                   [[maybe_unused]] argument_match_options options = {}) noexcept {
  if (arguments.size() != sizeof...(Arguments)) {
    return false;
  }

  [[maybe_unused]] std::size_t index = 0;
  return ((argument_rank<Arguments>(arguments[index++], options) != argument_match_rank::none) &&
          ...);
}

template <typename... Arguments>
[[nodiscard]] argument_match_rank
arguments_rank(std::span<const argument> arguments,
               [[maybe_unused]] argument_match_options options = {}) noexcept {
  if (arguments.size() != sizeof...(Arguments)) {
    return argument_match_rank::none;
  }

  argument_match_rank result = argument_match_rank::exact;
  [[maybe_unused]] std::size_t index = 0;
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

  vector<argument> result{memory::allocator<argument>{options.scratch_resource}};
  result.reserve(sizeof...(Arguments));
  [[maybe_unused]] std::size_t index = 0;
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

  return succeeded ? std::move(result)
                   : vector<argument>{memory::allocator<argument>{options.scratch_resource}};
}

} // namespace caors::meta::detail

#endif
