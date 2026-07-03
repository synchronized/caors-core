// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_internal_helpers.h"

namespace caors::serialization::detail {

[[nodiscard]] error read_sequence_into_view(reader& reader, const sequence_view& view,
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
  const meta::sequence_data* sequence =
      meta::get_registry().find_ptr<meta::sequence_data>(target.type());
  if (sequence != nullptr && !sequence->can_read) {
    return {error_domain::serialization, errc::unsupported};
  }

  const std::optional<meta::type_view> meta = caors::meta::find(target.type());
  if (sequence != nullptr && !sequence->read_in_place && meta.has_value() &&
      meta->assign_value() != nullptr) {
    any decoded = any::default_construct(target.type());
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

[[nodiscard]] error read_set_into_view(reader& reader, const set_view& view,
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

    any element = any::default_construct(view.value_type());
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
  const std::optional<meta::type_view> meta = caors::meta::find(target.type());
  if (meta.has_value() && meta->assign_value() != nullptr) {
    any decoded = any::default_construct(target.type());
    if (decoded.valid()) {
      error status = read_set_into_view(reader, decoded.create_set_view(), context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::unsupported};
      }

      return {};
    }
  }

  return read_set_into_view(reader, target.create_set_view(), context);
}

[[nodiscard]] error read_associative_into_view(reader& reader, const associative_view& view,
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

    any key = any::default_construct(view.key_type());
    any value = any::default_construct(view.value_type());
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
  const std::optional<meta::type_view> meta = caors::meta::find(target.type());
  if (meta.has_value() && meta->assign_value() != nullptr) {
    any decoded = any::default_construct(target.type());
    if (decoded.valid()) {
      error status = read_associative_into_view(reader, decoded.create_associative_view(), context);
      if (status) {
        return status;
      }

      if (!target.try_set(decoded)) {
        return {error_domain::serialization, errc::unsupported};
      }

      return {};
    }
  }

  return read_associative_into_view(reader, target.create_associative_view(), context);
}

[[nodiscard]] error read_pair_into_view(reader& reader, const pair_view& view,
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
  const std::optional<meta::type_view> meta = caors::meta::find(target.type());
  if (meta.has_value() && meta->assign_value() != nullptr) {
    any decoded = any::default_construct(target.type());
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

[[nodiscard]] error read_tuple_into_view(reader& reader, const tuple_view& view,
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
  const std::optional<meta::type_view> meta = caors::meta::find(target.type());
  if (meta.has_value() && meta->assign_value() != nullptr) {
    any decoded = any::default_construct(target.type());
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

[[nodiscard]] error read_variant_into_view(reader& reader, const variant_view& view,
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
  const std::optional<meta::type_view> meta = caors::meta::find(target.type());
  if (meta.has_value() && meta->assign_value() != nullptr) {
    any decoded = target.to_any();
    if (!decoded.valid()) {
      decoded = any::default_construct(target.type());
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
