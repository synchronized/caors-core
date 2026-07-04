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

  const caors::object* referenced = field.get_object_ref(object);
  const object_id id = referenced != nullptr
                           ? context.reference_resolver->object_to_id(
                                 referenced, context.reference_resolver->user_data)
                           : object_id::invalid();
  if (referenced != nullptr && !id) {
    return {error_domain::serialization, errc::not_found};
  }

  status = writer.write_u64(id.value());
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

  std::uint64_t encoded_id = 0;
  status = reader.read_u64(encoded_id);
  if (status) {
    return status;
  }

  caors::object* referenced = nullptr;
  if (encoded_id != 0) {
    referenced = context.reference_resolver->id_to_object(object_id{encoded_id},
                                                          context.reference_resolver->user_data);
    if (referenced == nullptr) {
      return {error_domain::serialization, errc::not_found};
    }
  }

  if (!field.try_set_object_ref(object, referenced)) {
    return {error_domain::serialization, errc::invalid_type};
  }

  return reader.end_field();
}

} // namespace caors::serialization::detail
