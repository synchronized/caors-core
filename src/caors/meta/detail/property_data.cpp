// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/detail/property_data.h"

#include "caors/meta/detail/value_conversion.h"
#include "caors/meta/value/value.h"

#include <optional>
#include <string>
#include <utility>

namespace caors::meta::detail {
namespace {

[[nodiscard]] bool try_assign_runtime_value(any_ref target, const meta::value& source) {
  if (!target) {
    return false;
  }

  if (source.is_ref()) {
    return target.try_set(source.as_ref());
  }

  if (source.is_any()) {
    const any* source_any = source.try_as_any();
    return source_any != nullptr && target.try_set(*source_any);
  }

  if (try_assign_value_to_known_ref<
          bool, char, signed char, unsigned char, short, unsigned short, int, unsigned int, long,
          unsigned long, long long, unsigned long long, float, double, long double,
          core::string_name, core::string, std::string, meta::object*>(target, source)) {
    return true;
  }

  any copied = source.to_any();
  return copied && target.try_set(copied);
}

template <typename Target>
[[nodiscard]] std::optional<any> try_make_exact_any_for_type(meta::type expected,
                                                             const meta::value& source) {
  using target_type = value_conversion_type<Target>;
  const meta::type actual = meta::resolve<target_type>();
  if (!expected || !actual || expected.id() != actual.id()) {
    return std::nullopt;
  }

  std::optional<target_type> converted = try_value_convert<target_type>(source);
  if (!converted.has_value()) {
    return std::nullopt;
  }

  return any::from(*converted);
}

template <typename... Targets>
[[nodiscard]] std::optional<any> try_make_exact_any_from_known(meta::type expected,
                                                               const meta::value& source) {
  std::optional<any> result{};
  (([&] {
     if (!result.has_value()) {
       result = try_make_exact_any_for_type<Targets>(expected, source);
     }
   }()),
   ...);
  return result;
}

[[nodiscard]] std::optional<any> try_make_exact_property_any(meta::type expected,
                                                             const meta::value& source) {
  if (!expected) {
    return std::nullopt;
  }

  if (source.is_ref()) {
    meta::any_ref ref = source.as_ref();
    if (ref && ref.get_type().id() == expected.id()) {
      return ref.to_any();
    }
    return std::nullopt;
  }

  if (source.is_any()) {
    const any* source_any = source.try_as_any();
    if (source_any != nullptr && source_any->valid() &&
        source_any->get_type().id() == expected.id()) {
      return *source_any;
    }
    return std::nullopt;
  }

  return try_make_exact_any_from_known<
      bool, char, signed char, unsigned char, short, unsigned short, int, unsigned int, long,
      unsigned long, long long, unsigned long long, float, double, long double, core::string_name,
      core::string, std::string, std::string_view, meta::object*>(expected, source);
}

[[nodiscard]] meta::value make_runtime_value(any_ref ref, any copied) {
  if (std::optional<meta::value> fast_value = try_make_value_from_ref(ref);
      fast_value.has_value()) {
    return std::move(*fast_value);
  }

  if (std::optional<meta::value> fast_value = try_make_value_from_any(copied);
      fast_value.has_value()) {
    return std::move(*fast_value);
  }
  return copied ? meta::value::from_any(std::move(copied)) : meta::value{};
}

} // namespace

meta::value property_data::get_runtime_value(const void* object) const {
  return make_runtime_value(get_ref(object), get_value(object));
}

meta::value property_data::get_runtime_value(void* object) const {
  return make_runtime_value(get_ref(object), get_value(object));
}

bool property_data::try_set_runtime_value(void* object, const meta::value& source) const {
  if (try_assign_runtime_value(get_ref(object), source)) {
    return true;
  }

  std::optional<any> exact = try_make_exact_property_any(get_type(), source);
  return exact.has_value() && try_set_value(object, *exact);
}

} // namespace caors::meta::detail
