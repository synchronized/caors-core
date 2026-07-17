// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include "caors/meta.h"

#include "detail/internal.h"

namespace caors::serialization::detail {

error serialize(const serialization::context& context, const meta::type& type, const void* object,
                writer& writer) {
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
    for (const serialization::field_schema& field_schema : serialization_data->fields) {
      if (field_schema.skipped ||
          !serialization::detail::field_visible_in_context(field_schema, context)) {
        continue;
      }

      const meta::property field = type.find_property(field_schema.field_name);
      if (!field) {
        return {error_domain::serialization, errc::not_found};
      }

      result<serialization::context> nested_context = context.enter_nested_value();
      if (nested_context.failed()) {
        return nested_context.status;
      }

      status = serialization::detail::write_field(writer, field, object, field_schema,
                                                  nested_context.value);
      if (status) {
        return status;
      }
    }
  } else {
    for (std::size_t index = 0; index < type.property_count(); ++index) {
      const meta::property field = type.property_at(index);
      if (!field) {
        return {error_domain::serialization, errc::invalid_field};
      }

      serialization::field_schema field_schema = serialization::make_field(field.name().view());
      result<serialization::context> nested_context = context.enter_nested_value();
      if (nested_context.failed()) {
        return nested_context.status;
      }

      status = serialization::detail::write_field(writer, field, object, field_schema,
                                                  nested_context.value);
      if (status) {
        return status;
      }
    }
  }

  return writer.end_object();
}

} // namespace caors::serialization::detail
