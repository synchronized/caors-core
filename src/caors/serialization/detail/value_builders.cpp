// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "helpers.h"

namespace caors::serialization::detail {

[[nodiscard]] variant_result make_sequence_value(const meta::sequence_view& view,
                                                 const serialization::context& context) {
  const std::optional<std::size_t> size = view.size();
  if (!view.valid() || !size.has_value()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  variant::array_type elements;
  elements.reserve(*size);
  for (std::size_t index = 0; index < *size; ++index) {
    any_ref element = view.element_ref(index);
    if (!element.valid()) {
      return {{error_domain::serialization, errc::unsupported}, {}};
    }

    result<serialization::context> nested_context = context.enter_nested_value();
    if (nested_context.failed()) {
      return {nested_context.status, {}};
    }

    variant_result encoded = make_serialized_value(element, nested_context.value);
    if (encoded.failed()) {
      return encoded;
    }

    elements.push_back(std::move(encoded.value));
  }

  return {{}, variant::array(std::move(elements))};
}

[[nodiscard]] variant_result make_set_value(const meta::set_view& view,
                                            const serialization::context& context) {
  const std::optional<std::size_t> size = view.size();
  if (!view.valid() || !size.has_value()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  variant::array_type elements;
  elements.reserve(*size);

  auto current = view.begin();
  auto last = view.end();
  if (*size != 0 && current == last) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  for (; current != last; ++current) {
    any_ref element = *current;
    if (!element.valid()) {
      return {{error_domain::serialization, errc::unsupported}, {}};
    }

    result<serialization::context> nested_context = context.enter_nested_value();
    if (nested_context.failed()) {
      return {nested_context.status, {}};
    }

    variant_result encoded = make_serialized_value(element, nested_context.value);
    if (encoded.failed()) {
      return encoded;
    }

    elements.push_back(std::move(encoded.value));
  }

  return {{}, variant::array(std::move(elements))};
}

[[nodiscard]] variant_result make_associative_value(const meta::associative_view& view,
                                                    const serialization::context& context) {
  const std::optional<std::size_t> size = view.size();
  if (!view.valid() || !size.has_value()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  variant::dictionary_type entries;
  entries.reserve(*size);

  auto current = view.begin();
  auto last = view.end();
  if (*size != 0 && current == last) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  for (; current != last; ++current) {
    const meta::associative_entry_ref entry = *current;
    any_ref key = entry.key();
    any_ref value = entry.value();
    if (!key.valid() || !value.valid()) {
      return {{error_domain::serialization, errc::unsupported}, {}};
    }

    result<serialization::context> nested_context = context.enter_nested_value();
    if (nested_context.failed()) {
      return {nested_context.status, {}};
    }

    variant_result encoded_key = make_serialized_value(key, nested_context.value);
    if (encoded_key.failed()) {
      return encoded_key;
    }

    variant_result encoded_value = make_serialized_value(value, nested_context.value);
    if (encoded_value.failed()) {
      return encoded_value;
    }

    entries.push_back(
        meta::variant_entry{std::move(encoded_key.value), std::move(encoded_value.value)});
  }

  return {{}, variant::dictionary(std::move(entries))};
}

[[nodiscard]] variant_result make_pair_value(const meta::pair_view& view,
                                             const serialization::context& context) {
  if (!view.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  any_ref first = view.first_ref();
  any_ref second = view.second_ref();
  if (!first.valid() || !second.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  result<serialization::context> nested_context = context.enter_nested_value();
  if (nested_context.failed()) {
    return {nested_context.status, {}};
  }

  variant_result encoded_first = make_serialized_value(first, nested_context.value);
  if (encoded_first.failed()) {
    return encoded_first;
  }

  variant_result encoded_second = make_serialized_value(second, nested_context.value);
  if (encoded_second.failed()) {
    return encoded_second;
  }

  variant::array_type elements;
  elements.reserve(2);
  elements.push_back(std::move(encoded_first.value));
  elements.push_back(std::move(encoded_second.value));

  return {{}, variant::array(std::move(elements))};
}

[[nodiscard]] variant_result make_tuple_value(const meta::tuple_view& view,
                                              const serialization::context& context) {
  if (!view.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  variant::array_type elements;
  const std::size_t size = view.size();
  elements.reserve(size);
  for (std::size_t index = 0; index < size; ++index) {
    any_ref element = view.element_ref(index);
    if (!element.valid()) {
      return {{error_domain::serialization, errc::unsupported}, {}};
    }

    result<serialization::context> nested_context = context.enter_nested_value();
    if (nested_context.failed()) {
      return {nested_context.status, {}};
    }

    variant_result encoded = make_serialized_value(element, nested_context.value);
    if (encoded.failed()) {
      return encoded;
    }

    elements.push_back(std::move(encoded.value));
  }

  return {{}, variant::array(std::move(elements))};
}

[[nodiscard]] variant_result make_variant_value(const meta::variant_view& view,
                                                const serialization::context& context) {
  const std::optional<std::size_t> active = view.index();
  if (!view.valid() || !active.has_value()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  any_ref value = view.value_ref();
  if (!value.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  result<serialization::context> nested_context = context.enter_nested_value();
  if (nested_context.failed()) {
    return {nested_context.status, {}};
  }

  variant_result encoded = make_serialized_value(value, nested_context.value);
  if (encoded.failed()) {
    return encoded;
  }

  variant::array_type elements;
  elements.reserve(2);
  elements.push_back(variant::from(static_cast<std::uint64_t>(*active)));
  elements.push_back(std::move(encoded.value));

  return {{}, variant::array(std::move(elements))};
}

} // namespace caors::serialization::detail
