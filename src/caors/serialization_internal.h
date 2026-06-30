// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_INTERNAL_H_
#define CAORS_SERIALIZATION_INTERNAL_H_

#include "caors/serialization.h"

namespace caors::detail {

[[nodiscard]] string_name serialized_field_name(const serialization_field_data& data) noexcept;
[[nodiscard]] bool field_visible_in_context(const serialization_field_data& field,
                                            const serialization_context& context) noexcept;
[[nodiscard]] error validate_field_for_writer(const serialization_field_data& field,
                                              serialization_format_features features);
[[nodiscard]] error validate_field_for_reader(const serialization_field_data& field,
                                              serialization_format_features features);
[[nodiscard]] bool try_write_serialized_value(serialization_writer& writer, const any& data);
[[nodiscard]] variant_result make_serialized_value(const any& data);
[[nodiscard]] error write_field(serialization_writer& writer, const reflection_field& field,
                                const void* object, const serialization_field_data& field_data,
                                const serialization_context& context);
[[nodiscard]] error read_field(serialization_reader& reader, const reflection_field& field,
                               void* object, const serialization_field_data& field_data,
                               const serialization_context& context);
[[nodiscard]] error read_configured_field(serialization_reader& reader,
                                          const reflection_field& field, void* object,
                                          const serialization_field_data& field_data,
                                          const serialization_context& context);

} // namespace caors::detail

#endif
