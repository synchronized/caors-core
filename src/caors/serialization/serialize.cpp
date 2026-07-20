// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include "caors/meta/property.h"
#include "caors/meta/type.h"

#include "detail/internal.h"
#include "detail/type_plan.h"

namespace caors::serialization::detail {

error serialize(const serialization::context& context, const meta::type& type, const void* object,
                writer& writer) {
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
  error status = writer.begin_object(plan.object_schema);
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

    status = serialization::detail::write_field(writer, field.property, object, field.schema,
                                                nested_context.value);
    if (status) {
      return status;
    }
  }

  return writer.end_object();
}

} // namespace caors::serialization::detail
