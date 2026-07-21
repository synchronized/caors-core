// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include "caors/meta/property.h"
#include "caors/meta/type.h"

#include "detail/internal.h"

namespace caors::serialization::detail {

error deserialize(const serialization::context& context, const meta::type& type, void* object,
                  reader& reader) {
  if (!type.valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (object == nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  const serialization::schema_view schema_view = type.get_capability<serialization::schema_view>();
  const serialization::type_schema* schema = schema_view.raw();
  if (schema == nullptr) {
    return {error_domain::serialization, errc::missing_type};
  }

  if (!schema_view.valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  error status = reader.begin_object(*schema);
  if (status) {
    return status;
  }

  for (const serialization::field_schema& field : schema->fields) {
    if (field.skipped || !serialization::detail::field_visible_in_context(field, context)) {
      continue;
    }

    if (!field.property) {
      return {error_domain::serialization, errc::not_found};
    }

    result<serialization::context> nested_context = context.enter_nested_value();
    if (nested_context.failed()) {
      return nested_context.status;
    }

    if (schema->has_explicit_fields) {
      status =
          serialization::detail::read_configured_field(reader, object, field, nested_context.value);
    } else {
      status = serialization::detail::read_field(reader, object, field, nested_context.value);
    }
    if (status) {
      return status;
    }
  }

  return reader.end_object();
}

} // namespace caors::serialization::detail
