// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/schema_registration.h"

#include "detail/internal.h"

namespace caors::serialization {

error prepare_type_schema(const meta::type& owner_type, type_schema& schema) {
  if (!owner_type || !owner_type.registered()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  schema.has_explicit_fields = !schema.fields.empty();
  if (!schema.valid()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (!schema.has_explicit_fields) {
    schema.fields.reserve(owner_type.property_count());
    for (std::size_t index = 0; index < owner_type.property_count(); ++index) {
      const meta::property property = owner_type.property_at(index);
      if (!property) {
        return {error_domain::serialization, errc::invalid_field};
      }
      schema.fields.emplace_back(make_field(property.name().view()));
    }
  }

  for (const field_schema& field : schema.fields) {
    if (!owner_type.find_property(field.field_name, meta::member_search::include_bases)) {
      return {error_domain::serialization, errc::not_found};
    }
  }

  return detail::compile_type_schema(owner_type, schema);
}

} // namespace caors::serialization
