// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "helpers.h"

namespace caors::serialization::detail {

[[nodiscard]] error write_object_ref_field(writer& writer, const reflection::field& field,
                                           const void* object,
                                           const serialization::field_data& field_data,
                                           const serialization::context& context) {
  if (context.reference_resolver == nullptr || !context.reference_resolver->can_write()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  error status = validate_field_for_writer(field_data, writer.features());
  if (status) {
    return status;
  }

  status = writer.begin_field(field_data);
  if (status) {
    return status;
  }

  status = write_object_ref_identity(writer, field.get_object_ref(object), context);
  if (status) {
    return status;
  }

  return writer.end_field();
}

[[nodiscard]] error read_object_ref_field(reader& reader, const reflection::field& field,
                                          void* object, const serialization::field_data& field_data,
                                          const serialization::context& context) {
  if (context.reference_resolver == nullptr || !context.reference_resolver->can_read()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  error status = validate_field_for_reader(field_data, reader.features());
  if (status) {
    return status;
  }

  status = reader.begin_field(field_data);
  if (status) {
    return status;
  }

  caors::object* referenced = nullptr;
  status = read_object_ref_identity(reader, context, referenced);
  if (status) {
    return status;
  }

  if (!field.try_set_object_ref(object, referenced)) {
    return {error_domain::serialization, errc::invalid_type};
  }

  return reader.end_field();
}

} // namespace caors::serialization::detail
