// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "detail/helpers.h"

namespace caors::serialization::detail {

[[nodiscard]] variant_result make_sequence_value(const value::sequence_view& view,
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

    variant_result encoded = make_serialized_value(element, context);
    if (encoded.failed()) {
      return encoded;
    }

    elements.push_back(std::move(encoded.value));
  }

  return {{}, variant::array(std::move(elements))};
}

[[nodiscard]] variant_result make_set_value(const value::set_view& view,
                                            const serialization::context& context) {
  const std::optional<std::size_t> size = view.size();
  if (!view.valid() || !size.has_value()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  variant::array_type elements;
  elements.reserve(*size);

  struct visitor_context {
    variant::array_type* elements = nullptr;
    const serialization::context* context = nullptr;
    variant_result result{};
  } visitor{&elements, &context, {}};

  const bool completed = view.for_each_ref(
      [](any_ref element, void* user_data) -> bool {
        auto* visitor = static_cast<visitor_context*>(user_data);
        variant_result encoded = make_serialized_value(element, *visitor->context);
        if (encoded.failed()) {
          visitor->result = encoded;
          return false;
        }

        visitor->elements->push_back(std::move(encoded.value));
        return true;
      },
      &visitor);

  if (!completed) {
    return visitor.result.failed()
               ? visitor.result
               : variant_result{{error_domain::serialization, errc::unsupported}, {}};
  }

  return {{}, variant::array(std::move(elements))};
}

[[nodiscard]] variant_result make_associative_value(const value::associative_view& view,
                                                    const serialization::context& context) {
  const std::optional<std::size_t> size = view.size();
  if (!view.valid() || !size.has_value()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  variant::dictionary_type entries;
  entries.reserve(*size);

  struct visitor_context {
    variant::dictionary_type* entries = nullptr;
    const serialization::context* context = nullptr;
    variant_result result{};
  } visitor{&entries, &context, {}};

  const bool completed = view.for_each_ref(
      [](any_ref key, any_ref value, void* user_data) -> bool {
        auto* visitor = static_cast<visitor_context*>(user_data);
        variant_result encoded_key = make_serialized_value(key, *visitor->context);
        if (encoded_key.failed()) {
          visitor->result = encoded_key;
          return false;
        }

        variant_result encoded_value = make_serialized_value(value, *visitor->context);
        if (encoded_value.failed()) {
          visitor->result = encoded_value;
          return false;
        }

        visitor->entries->push_back(
            value::variant_entry{std::move(encoded_key.value), std::move(encoded_value.value)});
        return true;
      },
      &visitor);

  if (!completed) {
    return visitor.result.failed()
               ? visitor.result
               : variant_result{{error_domain::serialization, errc::unsupported}, {}};
  }

  return {{}, variant::dictionary(std::move(entries))};
}

[[nodiscard]] variant_result make_pair_value(const value::pair_view& view,
                                             const serialization::context& context) {
  if (!view.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  any_ref first = view.first_ref();
  any_ref second = view.second_ref();
  if (!first.valid() || !second.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  variant_result encoded_first = make_serialized_value(first, context);
  if (encoded_first.failed()) {
    return encoded_first;
  }

  variant_result encoded_second = make_serialized_value(second, context);
  if (encoded_second.failed()) {
    return encoded_second;
  }

  variant::array_type elements;
  elements.reserve(2);
  elements.push_back(std::move(encoded_first.value));
  elements.push_back(std::move(encoded_second.value));

  return {{}, variant::array(std::move(elements))};
}

[[nodiscard]] variant_result make_tuple_value(const value::tuple_view& view,
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

    variant_result encoded = make_serialized_value(element, context);
    if (encoded.failed()) {
      return encoded;
    }

    elements.push_back(std::move(encoded.value));
  }

  return {{}, variant::array(std::move(elements))};
}

[[nodiscard]] variant_result make_variant_value(const value::variant_view& view,
                                                const serialization::context& context) {
  const std::optional<std::size_t> active = view.index();
  if (!view.valid() || !active.has_value()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  any_ref value = view.value_ref();
  if (!value.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  variant_result encoded = make_serialized_value(value, context);
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
