// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include "caors/reflection/registry.h"

#include "detail/internal.h"

namespace caors::serialization {

error serialize(const reflection::type& type, const void* object, writer& writer) {
  return serialize(type, object, writer, serialization::context{});
}

error serialize(const reflection::type& type, const void* object, writer& writer,
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

  error status = writer.begin_object(*serialization_data);
  if (status) {
    return status;
  }

  if (!serialization_data->fields.empty()) {
    for (const serialization::field_data& field_data : serialization_data->fields) {
      if (field_data.skipped ||
          !serialization::detail::field_visible_in_context(field_data, context)) {
        continue;
      }

      const reflection::field field = type.find_field(field_data.field_name);
      if (!field) {
        return {error_domain::serialization, errc::not_found};
      }

      status = serialization::detail::write_field(writer, field, object, field_data, context);
      if (status) {
        return status;
      }
    }
  } else {
    for (std::size_t index = 0; index < type.field_count(); ++index) {
      const reflection::field field = type.field_at(index);
      if (!field) {
        return {error_domain::serialization, errc::invalid_field};
      }

      serialization::field_data field_data = serialization::make_field(field.name().view());
      status = serialization::detail::write_field(writer, field, object, field_data, context);
      if (status) {
        return status;
      }
    }
  }

  return writer.end_object();
}

error serialize(type_id id, const void* object, writer& writer) {
  return serialize(id, object, writer, serialization::context{});
}

error serialize(type_id id, const void* object, writer& writer,
                const serialization::context& context) {
  if (!id) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (!serialization::contains(id)) {
    return {error_domain::serialization, errc::missing_type};
  }

  const reflection::type type = reflection::find(id);
  if (!type) {
    return {error_domain::serialization, errc::not_found};
  }

  return serialize(type, object, writer, context);
}

} // namespace caors::serialization
