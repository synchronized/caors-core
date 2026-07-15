// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "helpers.h"

namespace caors::serialization::detail {

[[nodiscard]] error write_sequence_value(writer& writer, const meta::sequence_view& view,
                                         const serialization::context& context) {
  const std::optional<std::size_t> size = view.size();
  if (!view.valid() || !size.has_value()) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = writer.begin_array(*size);
  if (status) {
    return status;
  }

  for (std::size_t index = 0; index < *size; ++index) {
    status = writer.begin_element(index);
    if (status) {
      return status;
    }

    any_ref element = view.element_ref(index);
    if (!element.valid()) {
      return {error_domain::serialization, errc::unsupported};
    }

    result<serialization::context> nested_context = context.enter_nested_value();
    if (nested_context.failed()) {
      return nested_context.status;
    }

    status = write_serialized_value(writer, element, nested_context.value);
    if (status) {
      return status;
    }

    status = writer.end_element();
    if (status) {
      return status;
    }
  }

  return writer.end_array();
}

[[nodiscard]] error write_set_value(writer& output, const meta::set_view& view,
                                    const serialization::context& context) {
  const std::optional<std::size_t> size = view.size();
  if (!view.valid() || !size.has_value()) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = output.begin_array(*size);
  if (status) {
    return status;
  }

  auto current = view.begin();
  auto last = view.end();
  if (*size != 0 && current == last) {
    return {error_domain::serialization, errc::unsupported};
  }

  std::size_t index = 0;
  for (; current != last; ++current) {
    status = output.begin_element(index);
    if (status) {
      return status;
    }

    any_ref element = *current;
    if (!element.valid()) {
      return {error_domain::serialization, errc::unsupported};
    }

    result<serialization::context> nested_context = context.enter_nested_value();
    if (nested_context.failed()) {
      return nested_context.status;
    }

    status = write_serialized_value(output, element, nested_context.value);
    if (status) {
      return status;
    }

    status = output.end_element();
    if (status) {
      return status;
    }

    ++index;
  }

  return output.end_array();
}

[[nodiscard]] error write_associative_value(writer& output, const meta::associative_view& view,
                                            const serialization::context& context) {
  const std::optional<std::size_t> size = view.size();
  if (!view.valid() || !size.has_value()) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = output.begin_dictionary(*size);
  if (status) {
    return status;
  }

  auto current = view.begin();
  auto last = view.end();
  if (*size != 0 && current == last) {
    return {error_domain::serialization, errc::unsupported};
  }

  std::size_t index = 0;
  for (; current != last; ++current) {
    const meta::associative_entry_ref entry = *current;
    any_ref key = entry.key();
    any_ref value = entry.value();
    if (!key.valid() || !value.valid()) {
      return {error_domain::serialization, errc::unsupported};
    }

    status = output.begin_entry(index);
    if (status) {
      return status;
    }

    status = output.begin_key();
    if (status) {
      return status;
    }

    result<serialization::context> nested_context = context.enter_nested_value();
    if (nested_context.failed()) {
      return nested_context.status;
    }

    status = write_serialized_value(output, key, nested_context.value);
    if (status) {
      return status;
    }

    status = output.end_key();
    if (status) {
      return status;
    }

    status = output.begin_value();
    if (status) {
      return status;
    }

    status = write_serialized_value(output, value, nested_context.value);
    if (status) {
      return status;
    }

    status = output.end_value();
    if (status) {
      return status;
    }

    status = output.end_entry();
    if (status) {
      return status;
    }

    ++index;
  }

  return output.end_dictionary();
}

[[nodiscard]] error write_pair_value(writer& writer, const meta::pair_view& view,
                                     const serialization::context& context) {
  if (!view.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = writer.begin_array(2);
  if (status) {
    return status;
  }

  status = writer.begin_element(0);
  if (status) {
    return status;
  }

  any_ref first = view.first_ref();
  if (!first.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  result<serialization::context> nested_context = context.enter_nested_value();
  if (nested_context.failed()) {
    return nested_context.status;
  }

  status = write_serialized_value(writer, first, nested_context.value);
  if (status) {
    return status;
  }

  status = writer.end_element();
  if (status) {
    return status;
  }

  status = writer.begin_element(1);
  if (status) {
    return status;
  }

  any_ref second = view.second_ref();
  if (!second.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  status = write_serialized_value(writer, second, nested_context.value);
  if (status) {
    return status;
  }

  status = writer.end_element();
  if (status) {
    return status;
  }

  return writer.end_array();
}

[[nodiscard]] error write_tuple_value(writer& writer, const meta::tuple_view& view,
                                      const serialization::context& context) {
  if (!view.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  const std::size_t size = view.size();
  error status = writer.begin_array(size);
  if (status) {
    return status;
  }

  for (std::size_t index = 0; index < size; ++index) {
    status = writer.begin_element(index);
    if (status) {
      return status;
    }

    any_ref element = view.element_ref(index);
    if (!element.valid()) {
      return {error_domain::serialization, errc::unsupported};
    }

    result<serialization::context> nested_context = context.enter_nested_value();
    if (nested_context.failed()) {
      return nested_context.status;
    }

    status = write_serialized_value(writer, element, nested_context.value);
    if (status) {
      return status;
    }

    status = writer.end_element();
    if (status) {
      return status;
    }
  }

  return writer.end_array();
}

[[nodiscard]] error write_variant_value(writer& writer, const meta::variant_view& view,
                                        const serialization::context& context) {
  const std::optional<std::size_t> active = view.index();
  if (!view.valid() || !active.has_value()) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = writer.begin_array(2);
  if (status) {
    return status;
  }

  status = writer.begin_element(0);
  if (status) {
    return status;
  }

  status = writer.write_u64(static_cast<std::uint64_t>(*active));
  if (status) {
    return status;
  }

  status = writer.end_element();
  if (status) {
    return status;
  }

  status = writer.begin_element(1);
  if (status) {
    return status;
  }

  any_ref value = view.value_ref();
  if (!value.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  result<serialization::context> nested_context = context.enter_nested_value();
  if (nested_context.failed()) {
    return nested_context.status;
  }

  status = write_serialized_value(writer, value, nested_context.value);
  if (status) {
    return status;
  }

  status = writer.end_element();
  if (status) {
    return status;
  }

  return writer.end_array();
}

} // namespace caors::serialization::detail
