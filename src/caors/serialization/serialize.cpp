// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include "caors/reflection/registry.h"

#include "detail/internal.h"

namespace caors::serialization::detail {

error serialize(const serialization::context& context, const reflection::type& type,
                const void* object, writer& writer) {
  if (!type.valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (object == nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  if (!serialization::contains(type.id())) {
    return {error_domain::serialization, errc::missing_type};
  }

  const std::optional<serialization::type_schema> serialization_data =
      serialization::find(type.id());
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

} // namespace caors::serialization::detail
