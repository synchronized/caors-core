// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_DETAIL_POLYMORPHIC_H_
#define CAORS_SERIALIZATION_DETAIL_POLYMORPHIC_H_

#include "caors/result.h"
#include "caors/serialization/types.h"

namespace caors::serialization::detail {

[[nodiscard]] core::string_name polymorphic_type_field();
[[nodiscard]] core::string_name polymorphic_value_field();
[[nodiscard]] const value::variant_member* find_variant_member(const variant& source,
                                                               core::string_name name) noexcept;
[[nodiscard]] bool is_polymorphic_envelope(const variant& source) noexcept;
[[nodiscard]] result<type_guid> read_polymorphic_type_guid(const variant& envelope);
[[nodiscard]] const variant* read_polymorphic_value(const variant& envelope) noexcept;
[[nodiscard]] result<meta::type> resolve_polymorphic_meta_type(type_id declared_base,
                                                               const variant& envelope);
[[nodiscard]] variant_result make_polymorphic_envelope(type_guid guid, variant stored_value);
[[nodiscard]] variant_result make_polymorphic_envelope(const meta::type& dynamic_type,
                                                       variant stored_value);

} // namespace caors::serialization::detail

#endif
