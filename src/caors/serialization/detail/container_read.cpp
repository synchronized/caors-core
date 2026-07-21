// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "helpers.h"

#include "construction.h"

#include <optional>

namespace caors::serialization::detail {

[[nodiscard]] error sequence_resize_failure(const meta::sequence_view& view,
                                            std::size_t size) noexcept {
  const std::size_t fixed_size = view.fixed_size();
  if (fixed_size != meta::dynamic_sequence_size && fixed_size != size) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  return {error_domain::serialization, errc::invalid_type};
}

[[nodiscard]] error read_sequence_into_view(reader& reader, const meta::sequence_view& view,
                                            const serialization::context& context) {
  if (!view.valid() || !view.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  std::size_t size = 0;
  error status = reader.begin_array(size);
  if (status) {
    return status;
  }

  if (!view.clear()) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (view.resize(size, context.effective_memory_resource())) {
    for (std::size_t index = 0; index < size; ++index) {
      status = reader.begin_element(index);
      if (status) {
        return status;
      }

      any_ref element = view.element_ref(index);
      if (!element.valid() || !element.writable()) {
        return {error_domain::serialization, errc::invalid_field};
      }

      serialization::context nested_context{};
      if (const error nested_status = context.enter_nested_value(nested_context); nested_status) {
        return nested_status;
      }

      status = read_serialized_value(reader, element, nested_context);
      if (status) {
        return status;
      }

      status = reader.end_element();
      if (status) {
        return status;
      }
    }

    return reader.end_array();
  }

  const error resize_error = sequence_resize_failure(view, size);
  if (resize_error.value() == errc::invalid_argument) {
    return resize_error;
  }

  for (std::size_t index = 0; index < size; ++index) {
    status = reader.begin_element(index);
    if (status) {
      return status;
    }

    any element =
        construct_value(view.get_value_type(), context, construction_site::sequence_element, index);
    if (!element.valid()) {
      return {error_domain::serialization, errc::invalid_type};
    }

    serialization::context nested_context{};
    if (const error nested_status = context.enter_nested_value(nested_context); nested_status) {
      return nested_status;
    }

    status = read_serialized_value(reader, element.ref(), nested_context);
    if (status) {
      return status;
    }

    if (!view.append_value(element)) {
      return {error_domain::serialization, errc::invalid_field};
    }

    status = reader.end_element();
    if (status) {
      return status;
    }
  }

  return reader.end_array();
}

[[nodiscard]] error read_sequence_value(reader& reader, const any_ref& target,
                                        const serialization::context& context) {
  const meta::sequence_view target_view = target.get_capability<meta::sequence_capability>();
  if (!target_view.can_read()) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (!target_view.read_in_place() && target.assignable()) {
    any decoded = default_construct_for(target, context);
    if (decoded.valid()) {
      error status = read_sequence_into_view(
          reader, decoded.get_capability<meta::sequence_capability>(), context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::invalid_field};
      }

      return {};
    }
  }

  return read_sequence_into_view(reader, target_view, context);
}

[[nodiscard]] error read_set_into_view(reader& reader, const meta::set_view& view,
                                       const serialization::context& context) {
  if (!view.valid() || !view.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  std::size_t size = 0;
  error status = reader.begin_array(size);
  if (status) {
    return status;
  }

  if (!view.clear()) {
    return {error_domain::serialization, errc::unsupported};
  }

  for (std::size_t index = 0; index < size; ++index) {
    status = reader.begin_element(index);
    if (status) {
      return status;
    }

    any element =
        construct_value(view.get_value_type(), context, construction_site::set_element, index);
    if (!element.valid()) {
      return {error_domain::serialization, errc::invalid_type};
    }

    serialization::context nested_context{};
    if (const error nested_status = context.enter_nested_value(nested_context); nested_status) {
      return nested_status;
    }

    status = read_serialized_value(reader, element.ref(), nested_context);
    if (status) {
      return status;
    }

    if (!view.insert_value(element)) {
      return {error_domain::serialization, errc::already_exists};
    }

    status = reader.end_element();
    if (status) {
      return status;
    }
  }

  return reader.end_array();
}

[[nodiscard]] error read_set_value(reader& reader, const any_ref& target,
                                   const serialization::context& context) {
  if (target.assignable()) {
    any decoded = default_construct_for(target, context);
    if (decoded.valid()) {
      error status =
          read_set_into_view(reader, decoded.get_capability<meta::set_capability>(), context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::invalid_field};
      }

      return {};
    }
  }

  return read_set_into_view(reader, target.get_capability<meta::set_capability>(), context);
}

[[nodiscard]] error read_associative_into_view(reader& reader, const meta::associative_view& view,
                                               const serialization::context& context) {
  if (!view.valid() || !view.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  std::size_t size = 0;
  error status = reader.begin_dictionary(size);
  if (status) {
    return status;
  }

  if (!view.clear()) {
    return {error_domain::serialization, errc::unsupported};
  }

  for (std::size_t index = 0; index < size; ++index) {
    status = reader.begin_entry(index);
    if (status) {
      return status;
    }

    any key =
        construct_value(view.get_key_type(), context, construction_site::associative_key, index);
    any value = construct_value(view.get_value_type(), context,
                                construction_site::associative_value, index);
    if (!key.valid() || !value.valid()) {
      return {error_domain::serialization, errc::invalid_type};
    }

    status = reader.begin_key();
    if (status) {
      return status;
    }

    serialization::context nested_context{};
    if (const error nested_status = context.enter_nested_value(nested_context); nested_status) {
      return nested_status;
    }

    status = read_serialized_value(reader, key.ref(), nested_context);
    if (status) {
      return status;
    }

    status = reader.end_key();
    if (status) {
      return status;
    }

    status = reader.begin_value();
    if (status) {
      return status;
    }

    status = read_serialized_value(reader, value.ref(), nested_context);
    if (status) {
      return status;
    }

    status = reader.end_value();
    if (status) {
      return status;
    }

    if (!view.insert_value(key, value)) {
      return {error_domain::serialization, errc::already_exists};
    }

    status = reader.end_entry();
    if (status) {
      return status;
    }
  }

  return reader.end_dictionary();
}

[[nodiscard]] error read_associative_value(reader& reader, const any_ref& target,
                                           const serialization::context& context) {
  if (target.assignable()) {
    any decoded = default_construct_for(target, context);
    if (decoded.valid()) {
      error status = read_associative_into_view(
          reader, decoded.get_capability<meta::associative_capability>(), context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::invalid_field};
      }

      return {};
    }
  }

  return read_associative_into_view(reader, target.get_capability<meta::associative_capability>(),
                                    context);
}

[[nodiscard]] error read_pair_into_view(reader& reader, const meta::pair_view& view,
                                        const serialization::context& context) {
  if (!view.valid() || !view.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  std::size_t size = 0;
  error status = reader.begin_array(size);
  if (status) {
    return status;
  }

  if (size != 2) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  status = reader.begin_element(0);
  if (status) {
    return status;
  }

  any_ref first = view.first_ref();
  if (!first.valid() || !first.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  serialization::context nested_context{};
  if (const error nested_status = context.enter_nested_value(nested_context); nested_status) {
    return nested_status;
  }

  status = read_serialized_value(reader, first, nested_context);
  if (status) {
    return status;
  }

  status = reader.end_element();
  if (status) {
    return status;
  }

  status = reader.begin_element(1);
  if (status) {
    return status;
  }

  any_ref second = view.second_ref();
  if (!second.valid() || !second.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  status = read_serialized_value(reader, second, nested_context);
  if (status) {
    return status;
  }

  status = reader.end_element();
  if (status) {
    return status;
  }

  return reader.end_array();
}

[[nodiscard]] error read_pair_value(reader& reader, const any_ref& target,
                                    const serialization::context& context) {
  if (target.assignable()) {
    any decoded = default_construct_for(target, context);
    if (decoded.valid()) {
      error status =
          read_pair_into_view(reader, decoded.get_capability<meta::pair_capability>(), context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::invalid_field};
      }

      return {};
    }
  }

  return read_pair_into_view(reader, target.get_capability<meta::pair_capability>(), context);
}

[[nodiscard]] error read_tuple_into_view(reader& reader, const meta::tuple_view& view,
                                         const serialization::context& context) {
  if (!view.valid() || !view.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  std::size_t size = 0;
  error status = reader.begin_array(size);
  if (status) {
    return status;
  }

  if (size != view.size()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  for (std::size_t index = 0; index < size; ++index) {
    status = reader.begin_element(index);
    if (status) {
      return status;
    }

    any_ref element = view.element_ref(index);
    if (!element.valid() || !element.writable()) {
      return {error_domain::serialization, errc::invalid_field};
    }

    serialization::context nested_context{};
    if (const error nested_status = context.enter_nested_value(nested_context); nested_status) {
      return nested_status;
    }

    status = read_serialized_value(reader, element, nested_context);
    if (status) {
      return status;
    }

    status = reader.end_element();
    if (status) {
      return status;
    }
  }

  return reader.end_array();
}

[[nodiscard]] error read_tuple_value(reader& reader, const any_ref& target,
                                     const serialization::context& context) {
  if (target.assignable()) {
    any decoded = default_construct_for(target, context);
    if (decoded.valid()) {
      error status =
          read_tuple_into_view(reader, decoded.get_capability<meta::tuple_capability>(), context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::invalid_field};
      }

      return {};
    }
  }

  return read_tuple_into_view(reader, target.get_capability<meta::tuple_capability>(), context);
}

[[nodiscard]] error read_variant_into_view(reader& reader, const meta::variant_view& view,
                                           const serialization::context& context) {
  if (!view.valid() || !view.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  std::size_t size = 0;
  error status = reader.begin_array(size);
  if (status) {
    return status;
  }

  if (size != 2) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  status = reader.begin_element(0);
  if (status) {
    return status;
  }

  std::size_t active = 0;
  status = read_size_value(reader, active);
  if (status) {
    return status;
  }

  if (active >= view.size()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  status = reader.end_element();
  if (status) {
    return status;
  }

  status = reader.begin_element(1);
  if (status) {
    return status;
  }

  serialization::context nested_context{};
  if (const error nested_status = context.enter_nested_value(nested_context); nested_status) {
    return nested_status;
  }

  if (view.emplace(active, context.effective_memory_resource())) {
    any_ref value = view.value_ref();
    if (!value.valid() || !value.writable()) {
      return {error_domain::serialization, errc::invalid_field};
    }

    status = read_serialized_value(reader, value, nested_context);
    if (status) {
      return status;
    }
  } else {
    const std::optional<meta::type> alternative_type = view.get_alternative_type(active);
    if (!alternative_type.has_value()) {
      return {error_domain::serialization, errc::invalid_argument};
    }

    any decoded =
        construct_value(*alternative_type, context, construction_site::variant_alternative, active);
    if (!decoded.valid()) {
      return {error_domain::serialization, errc::invalid_type};
    }

    status = read_serialized_value(reader, decoded.ref(), nested_context);
    if (status) {
      return status;
    }

    if (!view.set_value(active, decoded)) {
      return {error_domain::serialization, errc::invalid_field};
    }
  }

  status = reader.end_element();
  if (status) {
    return status;
  }

  return reader.end_array();
}

[[nodiscard]] error read_variant_value(reader& reader, const any_ref& target,
                                       const serialization::context& context) {
  if (target.assignable()) {
    any decoded = target.to_any();
    if (!decoded.valid()) {
      decoded = default_construct_for(target, context);
    }

    if (decoded.valid()) {
      error status = read_variant_into_view(
          reader, decoded.get_capability<meta::variant_capability>(), context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::invalid_field};
      }

      return {};
    }
  }

  return read_variant_into_view(reader, target.get_capability<meta::variant_capability>(), context);
}

} // namespace caors::serialization::detail
