// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_ENUM_VALUE_QUERY_H_
#define CAORS_META_DETAIL_ENUM_VALUE_QUERY_H_

#include "caors/meta/enum_value.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace caors::meta::detail {

[[nodiscard]] CAORS_API std::size_t enum_value_count(const type& source) noexcept;
[[nodiscard]] CAORS_API enum_value_view enum_value_at(const type& source, std::size_t index);
[[nodiscard]] CAORS_API enum_value_view find_enum_value(const type& source, string_name name);
[[nodiscard]] CAORS_API enum_value_view find_enum_value(const type& source, std::string_view name);
[[nodiscard]] CAORS_API enum_value_view find_enum_value_by_i64(const type& source,
                                                               std::int64_t value) noexcept;
[[nodiscard]] CAORS_API enum_value_view find_enum_value_by_u64(const type& source,
                                                               std::uint64_t value) noexcept;

} // namespace caors::meta::detail

#endif
