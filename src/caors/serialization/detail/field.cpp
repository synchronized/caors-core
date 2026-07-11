// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "helpers.h"

namespace caors::serialization::detail {

[[nodiscard]] error write_object_ptr_field(writer& writer, const reflection::field& field,
                                           const void* object,
                                           const serialization::field_schema& field_schema,
                                           const serialization::context& context) {
  const reference_resolver* resolver = context.effective_reference_resolver();
  if (resolver == nullptr || !resolver->can_write()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  error status = validate_field_for_writer(field_schema, writer.features());
  if (status) {
    return status;
  }

  status = writer.begin_field(field_schema);
  if (status) {
    return status;
  }

  status = write_object_ptr_identity(writer, field.get_object_ptr(object), context);
  if (status) {
    return status;
  }

  return writer.end_field();
}

[[nodiscard]] error read_object_ptr_field(reader& reader, const reflection::field& field,
                                          void* object,
                                          const serialization::field_schema& field_schema,
                                          const serialization::context& context) {
  const reference_resolver* resolver = context.effective_reference_resolver();
  if (resolver == nullptr || !resolver->can_read()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  error status = validate_field_for_reader(field_schema, reader.features());
  if (status) {
    return status;
  }

  status = reader.begin_field(field_schema);
  if (status) {
    return status;
  }

  caors::object* referenced = nullptr;
  status = read_object_ptr_identity(reader, context, referenced);
  if (status) {
    return status;
  }

  if (!field.try_set_object_ptr(object, referenced)) {
    return {error_domain::serialization, errc::invalid_type};
  }

  return reader.end_field();
}

} // namespace caors::serialization::detail
