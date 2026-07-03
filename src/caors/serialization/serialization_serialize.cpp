// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include "caors/reflection/registry.h"

#include "serialization_internal.h"

namespace caors {

error serialize(const reflection_type& type, const void* object, serialization_writer& writer) {
  return serialize(type, object, writer, serialization_context{});
}

error serialize(const reflection_type& type, const void* object, serialization_writer& writer,
                const serialization_context& context) {
  if (!type.valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (object == nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  if (!serialization::contains(type.id())) {
    return {error_domain::serialization, errc::missing_type};
  }

  const std::optional<serialization_type_data> serialization_data = serialization::find(type.id());
  if (!serialization_data.has_value() || !serialization_data->valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  error status = writer.begin_object(*serialization_data);
  if (status) {
    return status;
  }

  if (!serialization_data->fields.empty()) {
    for (const serialization_field_data& field_data : serialization_data->fields) {
      if (field_data.skipped || !detail::field_visible_in_context(field_data, context)) {
        continue;
      }

      const std::optional<reflection_field> field = type.find_field(field_data.field_name);
      if (!field.has_value()) {
        return {error_domain::serialization, errc::not_found};
      }

      status = detail::write_field(writer, *field, object, field_data, context);
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
      status = detail::write_field(writer, *field, object, field_data, context);
      if (status) {
        return status;
      }
    }
  }

  return writer.end_object();
}

error serialize(type_id id, const void* object, serialization_writer& writer) {
  return serialize(id, object, writer, serialization_context{});
}

error serialize(type_id id, const void* object, serialization_writer& writer,
                const serialization_context& context) {
  if (!id) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (!serialization::contains(id)) {
    return {error_domain::serialization, errc::missing_type};
  }

  const std::optional<reflection_type> type = reflection::find(id);
  if (!type.has_value()) {
    return {error_domain::serialization, errc::not_found};
  }

  return serialize(*type, object, writer, context);
}

} // namespace caors
