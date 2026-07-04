// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "detail/helpers.h"

namespace caors::serialization::detail {

[[nodiscard]] const meta::detail::sequence_data* sequence_data_of(const any_ref& target) noexcept {
  const meta::detail::type_descriptor* descriptor = value::detail::descriptor_of(target);
  return meta::detail::descriptor_capability<meta::detail::sequence_data>(descriptor);
}

[[nodiscard]] const meta::detail::set_data* set_data_of(const any_ref& target) noexcept {
  const meta::detail::type_descriptor* descriptor = value::detail::descriptor_of(target);
  return meta::detail::descriptor_capability<meta::detail::set_data>(descriptor);
}

[[nodiscard]] const meta::detail::associative_data*
associative_data_of(const any_ref& target) noexcept {
  const meta::detail::type_descriptor* descriptor = value::detail::descriptor_of(target);
  return meta::detail::descriptor_capability<meta::detail::associative_data>(descriptor);
}

[[nodiscard]] meta::detail::value_assign_fn assign_fn_of(const any_ref& target) noexcept {
  const meta::detail::type_descriptor* descriptor = value::detail::descriptor_of(target);
  if (descriptor != nullptr && descriptor->ops.assign != nullptr) {
    return descriptor->ops.assign;
  }

  return caors::meta::detail::get_assign_value_fn(caors::meta::find(target.type()));
}

[[nodiscard]] any default_construct_for(const any_ref& target) {
  any decoded =
      value::detail::default_construct(target.type(), value::detail::descriptor_of(target));
  if (!decoded.valid()) {
    decoded = any::default_construct(target.type());
  }

  return decoded;
}

[[nodiscard]] error read_sequence_into_view(reader& reader, const value::sequence_view& view,
                                            const serialization::context& context) {
  if (!view.valid() || !view.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  std::size_t size = 0;
  error status = reader.begin_array(size);
  if (status) {
    return status;
  }

  if (!view.clear() || !view.resize(size)) {
    return {error_domain::serialization, errc::unsupported};
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

    status = read_serialized_value(reader, element, context);
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

[[nodiscard]] error read_sequence_value(reader& reader, const any_ref& target,
                                        const serialization::context& context) {
  const meta::detail::sequence_data* sequence = sequence_data_of(target);
  if (sequence != nullptr && !sequence->can_read) {
    return {error_domain::serialization, errc::unsupported};
  }

  const auto assign = assign_fn_of(target);
  if (sequence != nullptr && !sequence->read_in_place && assign != nullptr) {
    any decoded = default_construct_for(target);
    if (decoded.valid()) {
      error status = read_sequence_into_view(reader, decoded.create_sequence_view(), context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::unsupported};
      }

      return {};
    }
  }

  return read_sequence_into_view(reader, target.create_sequence_view(), context);
}

[[nodiscard]] error read_set_into_view(reader& reader, const value::set_view& view,
                                       const meta::detail::set_data* set,
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

    const meta::detail::type_descriptor* value_descriptor =
        set != nullptr ? set->value_descriptor : nullptr;
    any element = value::detail::default_construct(view.value_type(), value_descriptor);
    if (!element.valid()) {
      element = any::default_construct(view.value_type());
    }
    if (!element.valid()) {
      return {error_domain::serialization, errc::unsupported};
    }

    status = read_serialized_value(reader, element.ref(), context);
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
  const meta::detail::set_data* set = set_data_of(target);
  const auto assign = assign_fn_of(target);
  if (assign != nullptr) {
    any decoded = default_construct_for(target);
    if (decoded.valid()) {
      error status = read_set_into_view(reader, decoded.create_set_view(), set, context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::unsupported};
      }

      return {};
    }
  }

  return read_set_into_view(reader, target.create_set_view(), set, context);
}

[[nodiscard]] error read_associative_into_view(reader& reader, const value::associative_view& view,
                                               const meta::detail::associative_data* associative,
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

    const meta::detail::type_descriptor* key_descriptor =
        associative != nullptr ? associative->key_descriptor : nullptr;
    const meta::detail::type_descriptor* value_descriptor =
        associative != nullptr ? associative->value_descriptor : nullptr;
    any key = value::detail::default_construct(view.key_type(), key_descriptor);
    any value = value::detail::default_construct(view.value_type(), value_descriptor);
    if (!key.valid()) {
      key = any::default_construct(view.key_type());
    }
    if (!value.valid()) {
      value = any::default_construct(view.value_type());
    }
    if (!key.valid() || !value.valid()) {
      return {error_domain::serialization, errc::unsupported};
    }

    status = reader.begin_key();
    if (status) {
      return status;
    }

    status = read_serialized_value(reader, key.ref(), context);
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

    status = read_serialized_value(reader, value.ref(), context);
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
  const meta::detail::associative_data* associative = associative_data_of(target);
  const auto assign = assign_fn_of(target);
  if (assign != nullptr) {
    any decoded = default_construct_for(target);
    if (decoded.valid()) {
      error status = read_associative_into_view(reader, decoded.create_associative_view(),
                                                associative, context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::unsupported};
      }

      return {};
    }
  }

  return read_associative_into_view(reader, target.create_associative_view(), associative, context);
}

[[nodiscard]] error read_pair_into_view(reader& reader, const value::pair_view& view,
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
    return {error_domain::serialization, errc::unsupported};
  }

  status = reader.begin_element(0);
  if (status) {
    return status;
  }

  any_ref first = view.first_ref();
  if (!first.valid() || !first.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  status = read_serialized_value(reader, first, context);
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

  status = read_serialized_value(reader, second, context);
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
  const auto assign = assign_fn_of(target);
  if (assign != nullptr) {
    any decoded = default_construct_for(target);
    if (decoded.valid()) {
      error status = read_pair_into_view(reader, decoded.create_pair_view(), context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::unsupported};
      }

      return {};
    }
  }

  return read_pair_into_view(reader, target.create_pair_view(), context);
}

[[nodiscard]] error read_tuple_into_view(reader& reader, const value::tuple_view& view,
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
    return {error_domain::serialization, errc::unsupported};
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

    status = read_serialized_value(reader, element, context);
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
  const auto assign = assign_fn_of(target);
  if (assign != nullptr) {
    any decoded = default_construct_for(target);
    if (decoded.valid()) {
      error status = read_tuple_into_view(reader, decoded.create_tuple_view(), context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::unsupported};
      }

      return {};
    }
  }

  return read_tuple_into_view(reader, target.create_tuple_view(), context);
}

[[nodiscard]] error read_variant_into_view(reader& reader, const value::variant_view& view,
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
    return {error_domain::serialization, errc::unsupported};
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
    return {error_domain::serialization, errc::unsupported};
  }

  status = reader.end_element();
  if (status) {
    return status;
  }

  if (!view.emplace(active)) {
    return {error_domain::serialization, errc::unsupported};
  }

  status = reader.begin_element(1);
  if (status) {
    return status;
  }

  any_ref value = view.value_ref();
  if (!value.valid() || !value.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  status = read_serialized_value(reader, value, context);
  if (status) {
    return status;
  }

  status = reader.end_element();
  if (status) {
    return status;
  }

  return reader.end_array();
}

[[nodiscard]] error read_variant_value(reader& reader, const any_ref& target,
                                       const serialization::context& context) {
  const auto assign = assign_fn_of(target);
  if (assign != nullptr) {
    any decoded = target.to_any();
    if (!decoded.valid()) {
      decoded = default_construct_for(target);
    }

    if (decoded.valid()) {
      error status = read_variant_into_view(reader, decoded.create_variant_view(), context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::unsupported};
      }

      return {};
    }
  }

  return read_variant_into_view(reader, target.create_variant_view(), context);
}

} // namespace caors::serialization::detail
