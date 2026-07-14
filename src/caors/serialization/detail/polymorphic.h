// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_DETAIL_POLYMORPHIC_H_
#define CAORS_SERIALIZATION_DETAIL_POLYMORPHIC_H_

#include "caors/result.h"
#include "caors/serialization/types.h"

namespace caors::serialization::detail {

[[nodiscard]] core::string_name polymorphic_type_field(const serialization::context& context);
[[nodiscard]] core::string_name polymorphic_value_field(const serialization::context& context);
[[nodiscard]] core::string_name polymorphic_type_name_field(const serialization::context& context);
[[nodiscard]] const meta::variant_member* find_variant_member(const variant& source,
                                                              core::string_name name) noexcept;
[[nodiscard]] bool is_polymorphic_envelope(const variant& source,
                                           const serialization::context& context) noexcept;
[[nodiscard]] result<type_guid> read_polymorphic_type_guid(const variant& envelope,
                                                           const serialization::context& context);
[[nodiscard]] const variant* read_polymorphic_value(const variant& envelope,
                                                    const serialization::context& context) noexcept;
[[nodiscard]] result<meta::type>
resolve_polymorphic_meta_type(type_id declared_base, const variant& envelope,
                              const serialization::context& context);
[[nodiscard]] variant_result make_polymorphic_envelope(type_guid guid, core::string_name type_name,
                                                       variant stored_value,
                                                       const serialization::context& context);
[[nodiscard]] variant_result make_polymorphic_envelope(const meta::type& dynamic_type,
                                                       variant stored_value,
                                                       const serialization::context& context);

} // namespace caors::serialization::detail

#endif
