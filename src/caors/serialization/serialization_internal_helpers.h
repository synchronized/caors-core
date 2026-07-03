// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_INTERNAL_HELPERS_H_
#define CAORS_SERIALIZATION_INTERNAL_HELPERS_H_

#include "serialization_internal.h"

namespace caors::serialization::detail {

[[nodiscard]] error read_size_value(reader& reader, std::size_t& value);
[[nodiscard]] bool field_uses_inline_indirect_value(const reflection::field& field) noexcept;
[[nodiscard]] bool field_uses_direct_value(const reflection::field& field) noexcept;
[[nodiscard]] bool type_uses_inline_indirect_value(type_id id) noexcept;
[[nodiscard]] bool type_is_enumeration(type_id id) noexcept;
[[nodiscard]] bool type_uses_object_ref(type_id id) noexcept;

[[nodiscard]] variant_result make_object_ref_value(const any_ref& value,
                                                   const serialization::context& context);
[[nodiscard]] error write_object_ref_value(writer& writer, const any_ref& value,
                                           const serialization::context& context);
[[nodiscard]] error read_object_ref_value(reader& reader, const any_ref& target,
                                          const serialization::context& context);
[[nodiscard]] variant_result make_enum_value(const any_ref& value);
[[nodiscard]] error write_enum_value(writer& writer, const any_ref& value);
[[nodiscard]] error read_enum_value(reader& reader, const any_ref& target);
[[nodiscard]] variant_result make_indirect_serialized_value(const indirect_view& indirect,
                                                            const serialization::context& context);
[[nodiscard]] error write_indirect_serialized_value(writer& writer, const indirect_view& indirect,
                                                    const serialization::context& context);
[[nodiscard]] error read_indirect_serialized_value(reader& reader, const indirect_view& indirect,
                                                   const serialization::context& context);

[[nodiscard]] error write_sequence_value(writer& writer, const sequence_view& view,
                                         const serialization::context& context);
[[nodiscard]] error write_set_value(writer& writer, const set_view& view,
                                    const serialization::context& context);
[[nodiscard]] error write_associative_value(writer& writer, const associative_view& view,
                                            const serialization::context& context);
[[nodiscard]] error write_pair_value(writer& writer, const pair_view& view,
                                     const serialization::context& context);
[[nodiscard]] error write_tuple_value(writer& writer, const tuple_view& view,
                                      const serialization::context& context);
[[nodiscard]] error write_variant_value(writer& writer, const variant_view& view,
                                        const serialization::context& context);

[[nodiscard]] error read_sequence_value(reader& reader, const any_ref& target,
                                        const serialization::context& context);
[[nodiscard]] error read_set_value(reader& reader, const any_ref& target,
                                   const serialization::context& context);
[[nodiscard]] error read_associative_value(reader& reader, const any_ref& target,
                                           const serialization::context& context);
[[nodiscard]] error read_pair_value(reader& reader, const any_ref& target,
                                    const serialization::context& context);
[[nodiscard]] error read_tuple_value(reader& reader, const any_ref& target,
                                     const serialization::context& context);
[[nodiscard]] error read_variant_value(reader& reader, const any_ref& target,
                                       const serialization::context& context);

[[nodiscard]] variant_result make_sequence_value(const sequence_view& view,
                                                 const serialization::context& context);
[[nodiscard]] variant_result make_set_value(const set_view& view,
                                            const serialization::context& context);
[[nodiscard]] variant_result make_associative_value(const associative_view& view,
                                                    const serialization::context& context);
[[nodiscard]] variant_result make_pair_value(const pair_view& view,
                                             const serialization::context& context);
[[nodiscard]] variant_result make_tuple_value(const tuple_view& view,
                                              const serialization::context& context);
[[nodiscard]] variant_result make_variant_value(const variant_view& view,
                                                const serialization::context& context);

[[nodiscard]] error write_object_ref_field(writer& writer, const reflection::field& field,
                                           const void* object,
                                           const serialization::field_data& field_data,
                                           const serialization::context& context);
[[nodiscard]] error read_object_ref_field(reader& reader, const reflection::field& field,
                                          void* object, const serialization::field_data& field_data,
                                          const serialization::context& context);

} // namespace caors::serialization::detail

#endif