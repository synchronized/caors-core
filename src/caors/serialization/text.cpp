// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

namespace caors::serialization::detail {

serialization_text_result serialize_text(const serialization::context& context,
                                         const reflection::type& type, const void* object) {
  serialization::text_writer writer;
  const error status = serialize(context, type, object, writer);
  if (status) {
    return {status, {}};
  }

  return {status, writer.text()};
}

} // namespace caors::serialization::detail
