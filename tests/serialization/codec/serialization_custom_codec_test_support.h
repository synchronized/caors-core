// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "serialization_test_support.h"

#include <limits>

namespace caors::tests::serialization {

struct policy_custom_codec_value {
  int value = 0;
};

struct policy_custom_codec_actor {
  policy_custom_codec_value value{};
};

} // namespace caors::tests::serialization

namespace caors::serialization {

template <> struct codec_traits<caors::tests::serialization::policy_custom_codec_value> {
  using value_type = caors::tests::serialization::policy_custom_codec_value;

  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static caors::error write(writer& writer, const value_type& value) {
    return writer.write_i64(static_cast<std::int64_t>(value.value + 100));
  }

  [[nodiscard]] static caors::error read(reader& reader, value_type& value) {
    std::int64_t decoded = 0;
    caors::error status = reader.read_i64(decoded);
    if (status) {
      std::uint64_t unsigned_decoded = 0;
      status = reader.read_u64(unsigned_decoded);
      if (status) {
        return status;
      }

      if (unsigned_decoded > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return {caors::error_domain::serialization, caors::errc::invalid_argument};
      }

      decoded = static_cast<std::int64_t>(unsigned_decoded);
    }

    value.value = static_cast<int>(decoded - 100);
    return {};
  }

  [[nodiscard]] static caors::serialization::variant_result make_value(const value_type& value) {
    return {{}, caors::variant::from(static_cast<std::int64_t>(value.value + 100))};
  }
};

} // namespace caors::serialization
