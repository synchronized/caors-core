// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include "serialization_internal.h"

namespace caors {

error deserialize(const reflection_type& type, void* object, serialization_reader& reader) {
  return deserialize(type, object, reader, serialization_context{});
}

error deserialize(const reflection_type& type, void* object, serialization_reader& reader,
                  const serialization_context& context) {
  if (!type.valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (object == nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  if (!get_serialization_registry().contains(type.id())) {
    return {error_domain::serialization, errc::missing_type};
  }

  const std::optional<serialization_type_data> serialization =
      get_serialization_registry().find_type(type.id());
  if (!serialization.has_value() || !serialization->valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  error status = reader.begin_object(*serialization);
  if (status) {
    return status;
  }

  if (!serialization->fields.empty()) {
    for (const serialization_field_data& field_data : serialization->fields) {
      if (field_data.skipped || !detail::field_visible_in_context(field_data, context)) {
        continue;
      }

      const std::optional<reflection_field> field = type.find_field(field_data.field_name);
      if (!field.has_value()) {
        return {error_domain::serialization, errc::not_found};
      }

      status = detail::read_configured_field(reader, *field, object, field_data, context);
      if (status) {
        return status;
      }
    }
  } else {
    for (std::size_t index = 0; index < type.field_count(); ++index) {
      const std::optional<reflection_field> field = type.field_at(index);
      if (!field.has_value()) {
        return {error_domain::serialization, errc::invalid_field};
      }

      serialization_field_data field_data = make_serialization_field(field->name().view());
      status = detail::read_field(reader, *field, object, field_data, context);
      if (status) {
        return status;
      }
    }
  }

  return reader.end_object();
}

error deserialize(type_id id, void* object, serialization_reader& reader) {
  return deserialize(id, object, reader, serialization_context{});
}

error deserialize(type_id id, void* object, serialization_reader& reader,
                  const serialization_context& context) {
  if (!id) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (!get_serialization_registry().contains(id)) {
    return {error_domain::serialization, errc::missing_type};
  }

  const std::optional<reflection_type> type = find_reflection(id);
  if (!type.has_value()) {
    return {error_domain::serialization, errc::not_found};
  }

  return deserialize(*type, object, reader, context);
}

} // namespace caors
