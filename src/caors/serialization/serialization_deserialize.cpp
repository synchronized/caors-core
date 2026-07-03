// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include "caors/reflection/registry.h"

#include "serialization_internal.h"

namespace caors {

error deserialize(const reflection::type& type, void* object, serialization_reader& reader) {
  return deserialize(type, object, reader, serialization::context{});
}

error deserialize(const reflection::type& type, void* object, serialization_reader& reader,
                  const serialization::context& context) {
  if (!type.valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (object == nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  if (!serialization::contains(type.id())) {
    return {error_domain::serialization, errc::missing_type};
  }

  const std::optional<serialization::type_data> serialization_data = serialization::find(type.id());
  if (!serialization_data.has_value() || !serialization_data->valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  error status = reader.begin_object(*serialization_data);
  if (status) {
    return status;
  }

  if (!serialization_data->fields.empty()) {
    for (const serialization::field_data& field_data : serialization_data->fields) {
      if (field_data.skipped || !detail::field_visible_in_context(field_data, context)) {
        continue;
      }

      const std::optional<reflection::field> field = type.find_field(field_data.field_name);
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
      const std::optional<reflection::field> field = type.field_at(index);
      if (!field.has_value()) {
        return {error_domain::serialization, errc::invalid_field};
      }

      serialization::field_data field_data = make_serialization_field(field->name().view());
      status = detail::read_field(reader, *field, object, field_data, context);
      if (status) {
        return status;
      }
    }
  }

  return reader.end_object();
}

error deserialize(type_id id, void* object, serialization_reader& reader) {
  return deserialize(id, object, reader, serialization::context{});
}

error deserialize(type_id id, void* object, serialization_reader& reader,
                  const serialization::context& context) {
  if (!id) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (!serialization::contains(id)) {
    return {error_domain::serialization, errc::missing_type};
  }

  const std::optional<reflection::type> type = reflection::find(id);
  if (!type.has_value()) {
    return {error_domain::serialization, errc::not_found};
  }

  return deserialize(*type, object, reader, context);
}

} // namespace caors
