// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include "caors/meta/property.h"
#include "caors/meta/type.h"

#include "detail/internal.h"
#include "detail/type_plan.h"

namespace caors::serialization::detail {

error deserialize(const serialization::context& context, const meta::type& type, void* object,
                  reader& reader) {
  if (!type.valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (object == nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  result<const serialization::detail::type_plan*> plan_result =
      serialization::detail::find_type_plan(type);
  if (plan_result.failed()) {
    return plan_result.status;
  }

  const serialization::detail::type_plan& plan = *plan_result.value;
  error status = reader.begin_object(plan.object_schema);
  if (status) {
    return status;
  }

  for (const serialization::detail::planned_field& field : plan.fields) {
    if (field.schema.skipped ||
        !serialization::detail::field_visible_in_context(field.schema, context)) {
      continue;
    }

    result<serialization::context> nested_context = context.enter_nested_value();
    if (nested_context.failed()) {
      return nested_context.status;
    }

    if (plan.has_explicit_fields) {
      status = serialization::detail::read_configured_field(reader, field.property, object,
                                                            field.schema, nested_context.value);
    } else {
      status = serialization::detail::read_field(reader, field.property, object, field.schema,
                                                 nested_context.value);
    }
    if (status) {
      return status;
    }
  }

  return reader.end_object();
}

} // namespace caors::serialization::detail
