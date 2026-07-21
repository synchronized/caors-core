// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_DETAIL_HELPERS_H_
#define CAORS_SERIALIZATION_DETAIL_HELPERS_H_

#include "internal.h"

#include "caors/meta/value/capabilities.h"

namespace caors::serialization::detail {

[[nodiscard]] error read_size_value(reader& reader, std::size_t& value);
[[nodiscard]] bool type_uses_inline_indirect_value(const any_ref& value) noexcept;
[[nodiscard]] bool type_is_enumeration(const any_ref& value) noexcept;
[[nodiscard]] variant_result make_enum_value(const any_ref& value);
[[nodiscard]] error write_enum_value(writer& writer, const any_ref& value);
[[nodiscard]] error read_enum_value(reader& reader, const any_ref& target);
[[nodiscard]] variant_result make_indirect_serialized_value(const meta::indirect_view& indirect,
                                                            const serialization::context& context);
[[nodiscard]] error write_indirect_serialized_value(writer& writer,
                                                    const meta::indirect_view& indirect,
                                                    const serialization::context& context);
[[nodiscard]] error read_indirect_serialized_value(reader& reader,
                                                   const meta::indirect_view& indirect,
                                                   const serialization::context& context);

[[nodiscard]] error write_sequence_value(writer& writer, const meta::sequence_view& view,
                                         const serialization::context& context);
[[nodiscard]] error write_set_value(writer& writer, const meta::set_view& view,
                                    const serialization::context& context);
[[nodiscard]] error write_associative_value(writer& writer, const meta::associative_view& view,
                                            const serialization::context& context);
[[nodiscard]] error write_pair_value(writer& writer, const meta::pair_view& view,
                                     const serialization::context& context);
[[nodiscard]] error write_tuple_value(writer& writer, const meta::tuple_view& view,
                                      const serialization::context& context);
[[nodiscard]] error write_variant_value(writer& writer, const meta::variant_view& view,
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

[[nodiscard]] variant_result make_sequence_value(const meta::sequence_view& view,
                                                 const serialization::context& context);
[[nodiscard]] variant_result make_set_value(const meta::set_view& view,
                                            const serialization::context& context);
[[nodiscard]] variant_result make_associative_value(const meta::associative_view& view,
                                                    const serialization::context& context);
[[nodiscard]] variant_result make_pair_value(const meta::pair_view& view,
                                             const serialization::context& context);
[[nodiscard]] variant_result make_tuple_value(const meta::tuple_view& view,
                                              const serialization::context& context);
[[nodiscard]] variant_result make_variant_value(const meta::variant_view& view,
                                                const serialization::context& context);

} // namespace caors::serialization::detail

#endif
