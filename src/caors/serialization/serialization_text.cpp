// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

namespace caors::serialization {

serialization_text_result serialize_text(const reflection::type& type, const void* object) {
  return serialize_text(type, object, serialization::context{});
}

serialization_text_result serialize_text(const reflection::type& type, const void* object,
                                         const serialization::context& context) {
  serialization::text_writer writer;
  const error status = serialize(type, object, writer, context);
  if (status) {
    return {status, {}};
  }

  return {status, writer.text()};
}

serialization_text_result serialize_text(type_id id, const void* object) {
  return serialize_text(id, object, serialization::context{});
}

serialization_text_result serialize_text(type_id id, const void* object,
                                         const serialization::context& context) {
  serialization::text_writer writer;
  const error status = serialize(id, object, writer, context);
  if (status) {
    return {status, {}};
  }

  return {status, writer.text()};
}

} // namespace caors::serialization
