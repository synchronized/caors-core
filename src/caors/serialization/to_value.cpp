// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include "caors/meta/property.h"
#include "caors/meta/type.h"
#include "caors/meta/value/any.h"
#include "caors/meta/value/any_ref.h"
#include "caors/meta/value/indirect_view.h"

#include "detail/helpers.h"
#include "detail/internal.h"

#include <utility>

namespace caors::serialization {
namespace {

[[nodiscard]] variant_result field_to_serialization_value(const serialization::field_schema& field,
                                                          const void* object,
                                                          const serialization::context& context) {
  serialization::context nested_context{};
  if (const error status = context.enter_nested_value(nested_context); status) {
    return {status, {}};
  }

  any_ref field_ref{};
  if (field.uses_inline_indirect_value) {
    const meta::indirect_view indirect = serialization::detail::get_field_ref(field, object)
                                             .get_capability<meta::indirect_capability>();
    return serialization::detail::make_indirect_serialized_value(indirect, nested_context);
  }

  if (!field.uses_direct_value) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  field_ref = serialization::detail::get_field_ref(field, object);

  if (!field_ref.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  return serialization::detail::make_serialized_value(field_ref, nested_context, &field);
}

} // namespace

variant_result to_variant(const any& data) {
  return serialization::detail::make_serialized_value(data);
}

variant_result to_variant(const any_ref& data) {
  if (!data.valid()) {
    return {{error_domain::serialization, errc::invalid_argument}, {}};
  }

  return serialization::detail::make_serialized_value(data, serialization::context{});
}

namespace detail {

variant_result to_serialization_value(const serialization::context& context, const meta::type& type,
                                      const void* object) {
  if (!type.valid()) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  if (object == nullptr) {
    return {{error_domain::serialization, errc::invalid_argument}, {}};
  }

  const serialization::schema_view schema_view = type.get_capability<serialization::schema_view>();
  const serialization::type_schema* schema = schema_view.raw();
  if (schema == nullptr) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  if (!schema_view.valid()) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  variant::object_type members;
  members.reserve(schema->fields.size());

  for (const serialization::field_schema& field : schema->fields) {
    if (field.skipped || !serialization::detail::field_visible_in_context(field, context)) {
      continue;
    }

    if (!field.property) {
      return {{error_domain::serialization, errc::not_found}, {}};
    }

    variant_result serialized = field_to_serialization_value(field, object, context);
    if (serialized.failed()) {
      return serialized;
    }

    members.push_back(meta::variant_member{
        serialization::detail::serialized_field_name(field),
        field.field_id,
        std::move(serialized.value),
    });
  }

  return {{}, variant::object(std::move(members))};
}

} // namespace detail

} // namespace caors::serialization
