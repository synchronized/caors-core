// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_DETAIL_INTERNAL_H_
#define CAORS_SERIALIZATION_DETAIL_INTERNAL_H_

#include "caors/serialization.h"

#include "caors/meta/property.h"

namespace caors::serialization::detail {

/** @brief 序列化内部按字段 schema 和 meta type 编译出的运行期字段信息。 */
struct field_runtime_schema {
  const serialization::field_schema* schema = nullptr;
  meta::property property{};
  meta::type owner_type{};
  meta::type declared_type{};
  meta::type value_type{};
  const serialization::codec_ops* codec_operations = nullptr;
  const meta::scalar_ops* scalar_operations = nullptr;
  bool uses_direct_value = false;
  bool uses_inline_indirect_value = false;

  [[nodiscard]] bool valid() const noexcept { return schema != nullptr && property.valid(); }
};

[[nodiscard]] field_runtime_schema compile_field_schema(const meta::type& owner_type,
                                                        const serialization::field_schema& field);
[[nodiscard]] core::string_name
serialized_field_name(const serialization::field_schema& data) noexcept;
[[nodiscard]] bool field_visible_in_context(const serialization::field_schema& field,
                                            const serialization::context& context) noexcept;
[[nodiscard]] error validate_field_for_writer(const serialization::field_schema& field,
                                              serialization::format_features features);
[[nodiscard]] error validate_field_for_reader(const serialization::field_schema& field,
                                              serialization::format_features features);
[[nodiscard]] bool try_write_scalar_value(writer& writer, const any& data);
[[nodiscard]] bool try_write_scalar_value(writer& writer, const any_ref& data);
[[nodiscard]] bool try_write_scalar_value(writer& writer, const any_ref& data,
                                          const meta::scalar_ops* ops);
[[nodiscard]] variant_result make_scalar_value(const any_ref& data);
[[nodiscard]] variant_result make_scalar_value(const any_ref& data, const meta::scalar_ops* ops);
[[nodiscard]] error read_scalar_value(reader& reader, const any_ref& target,
                                      const serialization::context& context);
[[nodiscard]] error read_scalar_value(reader& reader, const any_ref& target,
                                      const serialization::context& context,
                                      const meta::scalar_ops* ops);
[[nodiscard]] any_ref get_field_ref(const field_runtime_schema& field_schema, const void* object);
[[nodiscard]] any_ref get_field_ref(const field_runtime_schema& field_schema, void* object);
[[nodiscard]] variant_result make_serialized_value(const any& data);
[[nodiscard]] variant_result make_serialized_value(const any_ref& value,
                                                   const serialization::context& context);
[[nodiscard]] variant_result make_serialized_value(const any_ref& value,
                                                   const serialization::context& context,
                                                   const field_runtime_schema* field);
[[nodiscard]] variant_result parse_json_value(std::string_view text, json_options options);
[[nodiscard]] variant_result parse_json_stream(io::input_stream& stream, json_options options);
[[nodiscard]] error write_serialized_value(writer& writer, const any_ref& value,
                                           const serialization::context& context);
[[nodiscard]] error write_serialized_value(writer& writer, const any_ref& value,
                                           const serialization::context& context,
                                           const field_runtime_schema* field);
[[nodiscard]] error read_serialized_value(reader& reader, const any_ref& target,
                                          const serialization::context& context);
[[nodiscard]] error read_serialized_value(reader& reader, const any_ref& target,
                                          const serialization::context& context,
                                          const field_runtime_schema* field);
[[nodiscard]] error write_field(writer& writer, const void* object,
                                const field_runtime_schema& field_schema,
                                const serialization::context& context);
[[nodiscard]] error read_field(reader& reader, void* object,
                               const field_runtime_schema& field_schema,
                               const serialization::context& context);
[[nodiscard]] error read_configured_field(reader& reader, void* object,
                                          const field_runtime_schema& field_schema,
                                          const serialization::context& context);
[[nodiscard]] error read_missing_tolerant_field(reader& reader, void* object,
                                                const field_runtime_schema& field_schema,
                                                const serialization::context& context);

} // namespace caors::serialization::detail

#endif
