// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_DETAIL_HELPERS_H_
#define CAORS_SERIALIZATION_DETAIL_HELPERS_H_

#include "internal.h"

#include "caors/value/view_factory.h"

namespace caors::serialization::detail {

[[nodiscard]] error read_size_value(reader& reader, std::size_t& value);
[[nodiscard]] bool field_uses_inline_indirect_value(const reflection::field& field) noexcept;
[[nodiscard]] bool field_uses_direct_value(const reflection::field& field) noexcept;
[[nodiscard]] bool type_uses_inline_indirect_value(const any_ref& value) noexcept;
[[nodiscard]] bool type_is_enumeration(const any_ref& value) noexcept;
[[nodiscard]] bool type_uses_object_handle(const any_ref& value) noexcept;
[[nodiscard]] bool type_uses_object_ptr(const any_ref& value) noexcept;

[[nodiscard]] variant_result make_object_handle_identity_value(objects::handle value);
[[nodiscard]] error write_object_handle_identity(writer& writer, objects::handle value);
[[nodiscard]] error read_object_handle_identity(reader& reader, objects::handle& value);

[[nodiscard]] variant_result make_object_handle_value(const any_ref& value);
[[nodiscard]] error write_object_handle_value(writer& writer, const any_ref& value);
[[nodiscard]] error read_object_handle_value(reader& reader, const any_ref& target);

[[nodiscard]] variant_result make_object_ptr_identity_value(const object* referenced,
                                                            const serialization::context& context);
[[nodiscard]] error write_object_ptr_identity(writer& writer, const object* referenced,
                                              const serialization::context& context);
[[nodiscard]] error read_object_ptr_identity(reader& reader, const serialization::context& context,
                                             object*& referenced);

[[nodiscard]] variant_result make_object_ptr_value(const any_ref& value,
                                                   const serialization::context& context);
[[nodiscard]] error write_object_ptr_value(writer& writer, const any_ref& value,
                                           const serialization::context& context);
[[nodiscard]] error read_object_ptr_value(reader& reader, const any_ref& target,
                                          const serialization::context& context);
[[nodiscard]] variant_result make_enum_value(const any_ref& value);
[[nodiscard]] error write_enum_value(writer& writer, const any_ref& value);
[[nodiscard]] error read_enum_value(reader& reader, const any_ref& target);
[[nodiscard]] variant_result make_indirect_serialized_value(const value::indirect_view& indirect,
                                                            const serialization::context& context);
[[nodiscard]] error write_indirect_serialized_value(writer& writer,
                                                    const value::indirect_view& indirect,
                                                    const serialization::context& context);
[[nodiscard]] error read_indirect_serialized_value(reader& reader,
                                                   const value::indirect_view& indirect,
                                                   const serialization::context& context);

[[nodiscard]] error write_sequence_value(writer& writer, const value::sequence_view& view,
                                         const serialization::context& context);
[[nodiscard]] error write_set_value(writer& writer, const value::set_view& view,
                                    const serialization::context& context);
[[nodiscard]] error write_associative_value(writer& writer, const value::associative_view& view,
                                            const serialization::context& context);
[[nodiscard]] error write_pair_value(writer& writer, const value::pair_view& view,
                                     const serialization::context& context);
[[nodiscard]] error write_tuple_value(writer& writer, const value::tuple_view& view,
                                      const serialization::context& context);
[[nodiscard]] error write_variant_value(writer& writer, const value::variant_view& view,
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

[[nodiscard]] variant_result make_sequence_value(const value::sequence_view& view,
                                                 const serialization::context& context);
[[nodiscard]] variant_result make_set_value(const value::set_view& view,
                                            const serialization::context& context);
[[nodiscard]] variant_result make_associative_value(const value::associative_view& view,
                                                    const serialization::context& context);
[[nodiscard]] variant_result make_pair_value(const value::pair_view& view,
                                             const serialization::context& context);
[[nodiscard]] variant_result make_tuple_value(const value::tuple_view& view,
                                              const serialization::context& context);
[[nodiscard]] variant_result make_variant_value(const value::variant_view& view,
                                                const serialization::context& context);

[[nodiscard]] error write_object_ptr_field(writer& writer, const reflection::field& field,
                                           const void* object,
                                           const serialization::field_data& field_data,
                                           const serialization::context& context);
[[nodiscard]] error read_object_ptr_field(reader& reader, const reflection::field& field,
                                          void* object, const serialization::field_data& field_data,
                                          const serialization::context& context);

} // namespace caors::serialization::detail

#endif
