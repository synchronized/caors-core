// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include "caors/meta/property.h"
#include "caors/meta/type.h"

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

  const serialization::schema_view schema_view =
      type.get_capability<serialization::schema_capability>();
  const serialization::type_schema* schema = schema_view.raw();
  if (schema == nullptr) {
    return {error_domain::serialization, errc::missing_type};
  }

  if (!schema_view.valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }
  const serialization::detail::type_runtime_schema* runtime =
      serialization::detail::runtime_schema(*schema);
  if (runtime == nullptr || runtime->fields.size() != schema->fields.size()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  error status = writer.begin_object(*schema);
  if (status) {
    return status;
  }

  for (const serialization::detail::field_runtime_schema& runtime_field : runtime->fields) {
    const serialization::field_schema& field = runtime_field.schema;
    if (field.skipped || !serialization::detail::field_visible_in_context(field, context)) {
      continue;
    }

    if (!runtime_field.valid()) {
      return {error_domain::serialization, errc::not_found};
    }

    serialization::context nested_context{};
    if (const error nested_status = context.enter_nested_value(nested_context); nested_status) {
      return nested_status;
    }

    status = serialization::detail::write_field(writer, object, runtime_field, nested_context);
    if (status) {
      return status;
    }
  }

  return writer.end_object();
}

} // namespace caors::serialization::detail
