// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_internal.h"

namespace caors::detail {

string_name serialized_field_name(const serialization_field_data& data) noexcept {
  return data.serialized_name.empty() ? data.field_name : data.serialized_name;
}

bool field_visible_in_context(const serialization_field_data& field,
                              const serialization_context& context) noexcept {
  if (context.schema_version == 0) {
    return true;
  }

  if (field.version_added != 0 && context.schema_version < field.version_added) {
    return false;
  }

  if (field.version_removed != 0 && context.schema_version >= field.version_removed) {
    return false;
  }

  return true;
}

error validate_field_for_writer(const serialization_field_data& field,
                                serialization_format_features features) {
  if (has_feature(features, serialization_format_features::named_fields) &&
      serialized_field_name(field).empty()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (has_feature(features, serialization_format_features::numeric_fields) && field.field_id == 0) {
    return {error_domain::serialization, errc::invalid_field};
  }

  return {};
}

error validate_field_for_reader(const serialization_field_data& field,
                                serialization_format_features features) {
  const bool can_use_name = has_feature(features, serialization_format_features::named_fields) &&
                            !serialized_field_name(field).empty();
  const bool can_use_id =
      has_feature(features, serialization_format_features::numeric_fields) && field.field_id != 0;

  if (!can_use_name && !can_use_id) {
    return {error_domain::serialization, errc::invalid_field};
  }

  return {};
}

} // namespace caors::detail
