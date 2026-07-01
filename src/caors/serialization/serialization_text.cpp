// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

namespace caors {

serialization_text_result serialize_text(const reflection_type& type, const void* object) {
  return serialize_text(type, object, serialization_context{});
}

serialization_text_result serialize_text(const reflection_type& type, const void* object,
                                         const serialization_context& context) {
  text_serialization_writer writer;
  const error status = serialize(type, object, writer, context);
  if (status) {
    return {status, {}};
  }

  return {status, writer.text()};
}

serialization_text_result serialize_text(type_id id, const void* object) {
  return serialize_text(id, object, serialization_context{});
}

serialization_text_result serialize_text(type_id id, const void* object,
                                         const serialization_context& context) {
  text_serialization_writer writer;
  const error status = serialize(id, object, writer, context);
  if (status) {
    return {status, {}};
  }

  return {status, writer.text()};
}

} // namespace caors
