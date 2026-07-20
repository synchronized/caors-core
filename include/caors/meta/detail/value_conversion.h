// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_VALUE_CONVERSION_H_
#define CAORS_META_DETAIL_VALUE_CONVERSION_H_

#include "caors/core/containers.h"
#include "caors/core/string_name.h"
#include "caors/meta/object.h"
#include "caors/meta/value/value.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>

namespace caors::meta::detail {

template <typename T> using value_conversion_type = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename T>
inline constexpr bool value_conversion_supported_v =
    std::is_same_v<value_conversion_type<T>, bool> ||
    (std::is_integral_v<value_conversion_type<T>> &&
     !std::is_same_v<value_conversion_type<T>, bool>) ||
    std::is_floating_point_v<value_conversion_type<T>> ||
    std::is_same_v<value_conversion_type<T>, core::string_name> ||
    std::is_same_v<value_conversion_type<T>, core::string> ||
    std::is_same_v<value_conversion_type<T>, std::string_view> ||
    std::is_same_v<value_conversion_type<T>, meta::object*>;

template <typename T>
inline constexpr bool value_parameter_conversion_supported_v =
    !std::is_reference_v<T> && value_conversion_supported_v<value_conversion_type<T>>;

template <typename Target>
[[nodiscard]] bool value_can_convert(const meta::value& source) noexcept {
  using target_type = value_conversion_type<Target>;

  if constexpr (std::is_same_v<target_type, bool>) {
    return source.is_bool();
  } else if constexpr (std::is_integral_v<target_type> && !std::is_same_v<target_type, bool>) {
    if constexpr (std::is_signed_v<target_type>) {
      if (const std::int64_t* value = source.try_as_i64(); value != nullptr) {
        return *value >= static_cast<std::int64_t>(std::numeric_limits<target_type>::min()) &&
               *value <= static_cast<std::int64_t>(std::numeric_limits<target_type>::max());
      }
      if (const std::uint64_t* value = source.try_as_u64(); value != nullptr) {
        return *value <= static_cast<std::uint64_t>(std::numeric_limits<target_type>::max());
      }
      return false;
    } else {
      if (const std::uint64_t* value = source.try_as_u64(); value != nullptr) {
        return *value <= static_cast<std::uint64_t>(std::numeric_limits<target_type>::max());
      }
      if (const std::int64_t* value = source.try_as_i64(); value != nullptr) {
        return *value >= 0 &&
               static_cast<std::uint64_t>(*value) <=
                   static_cast<std::uint64_t>(std::numeric_limits<target_type>::max());
      }
      return false;
    }
  } else if constexpr (std::is_floating_point_v<target_type>) {
    if (source.is_f64() || source.is_i64() || source.is_u64()) {
      return true;
    }
    return false;
  } else if constexpr (std::is_same_v<target_type, core::string_name>) {
    return source.is_string_name();
  } else if constexpr (std::is_same_v<target_type, core::string> ||
                       std::is_same_v<target_type, std::string_view>) {
    return source.is_text();
  } else if constexpr (std::is_same_v<target_type, meta::object*>) {
    return source.is_object();
  } else {
    return false;
  }
}

template <typename Target>
[[nodiscard]] std::optional<value_conversion_type<Target>>
try_value_convert(const meta::value& source) {
  using target_type = value_conversion_type<Target>;

  if (!value_can_convert<target_type>(source)) {
    return std::nullopt;
  }

  if constexpr (std::is_same_v<target_type, bool>) {
    return source.as_bool();
  } else if constexpr (std::is_integral_v<target_type> && !std::is_same_v<target_type, bool>) {
    if constexpr (std::is_signed_v<target_type>) {
      if (const std::int64_t* value = source.try_as_i64(); value != nullptr) {
        return static_cast<target_type>(*value);
      }
      return static_cast<target_type>(*source.try_as_u64());
    } else {
      if (const std::uint64_t* value = source.try_as_u64(); value != nullptr) {
        return static_cast<target_type>(*value);
      }
      return static_cast<target_type>(*source.try_as_i64());
    }
  } else if constexpr (std::is_floating_point_v<target_type>) {
    if (const double* value = source.try_as_f64(); value != nullptr) {
      return static_cast<target_type>(*value);
    }
    if (const std::int64_t* value = source.try_as_i64(); value != nullptr) {
      return static_cast<target_type>(*value);
    }
    return static_cast<target_type>(*source.try_as_u64());
  } else if constexpr (std::is_same_v<target_type, core::string_name>) {
    return source.as_string_name();
  } else if constexpr (std::is_same_v<target_type, core::string>) {
    const std::optional<std::string_view> view = source.try_as_string_view();
    if (!view.has_value()) {
      return std::nullopt;
    }
    return target_type{*view};
  } else if constexpr (std::is_same_v<target_type, std::string_view>) {
    return source.try_as_string_view();
  } else if constexpr (std::is_same_v<target_type, meta::object*>) {
    return source.as_object();
  } else {
    return std::nullopt;
  }
}

template <typename Target>
[[nodiscard]] bool try_assign_value_to_ref(meta::any_ref target, const meta::value& source) {
  if (!target || !target.writable()) {
    return false;
  }

  if (source.is_ref()) {
    return target.try_set(source.as_ref());
  }

  if (source.is_any()) {
    const meta::any* source_any = source.try_as_any();
    return source_any != nullptr && target.try_set(*source_any);
  }

  if constexpr (value_conversion_supported_v<Target>) {
    std::optional<value_conversion_type<Target>> converted = try_value_convert<Target>(source);
    return converted.has_value() && target.try_set(*converted);
  } else {
    return false;
  }
}

template <typename... Targets>
[[nodiscard]] bool try_assign_value_to_known_ref(meta::any_ref target, const meta::value& source) {
  return (try_assign_value_to_ref<Targets>(target, source) || ...);
}

template <typename Target, typename Source>
[[nodiscard]] std::optional<meta::value> try_make_value_from_exact(const Source& source) {
  if (const value_conversion_type<Target>* value =
          source.template try_get<value_conversion_type<Target>>();
      value != nullptr) {
    if constexpr (std::is_same_v<value_conversion_type<Target>, meta::object*>) {
      return meta::value::from_object(*value);
    } else {
      return meta::value{*value};
    }
  }

  return std::nullopt;
}

template <typename... Targets, typename Source>
[[nodiscard]] std::optional<meta::value> try_make_value_from_known(const Source& source) {
  std::optional<meta::value> result{};
  (([&] {
     if (!result.has_value()) {
       result = try_make_value_from_exact<Targets>(source);
     }
   }()),
   ...);
  return result;
}

[[nodiscard]] inline std::optional<meta::value> try_make_value_from_ref(meta::any_ref source) {
  if (!source) {
    return std::nullopt;
  }

  return try_make_value_from_known<
      bool, char, signed char, unsigned char, short, unsigned short, int, unsigned int, long,
      unsigned long, long long, unsigned long long, float, double, long double, core::string_name,
      core::string, std::string_view, meta::object*>(source);
}

[[nodiscard]] inline std::optional<meta::value> try_make_value_from_any(const meta::any& source) {
  if (!source) {
    return std::nullopt;
  }

  return try_make_value_from_known<
      bool, char, signed char, unsigned char, short, unsigned short, int, unsigned int, long,
      unsigned long, long long, unsigned long long, float, double, long double, core::string_name,
      core::string, std::string_view, meta::object*>(source);
}

} // namespace caors::meta::detail

#endif
