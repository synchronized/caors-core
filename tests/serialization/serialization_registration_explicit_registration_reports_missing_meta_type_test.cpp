// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - explicit registration reports missing meta type", "[serialization]") {
  const caors::error status = caors::serialization::register_type(
      caors::type_id{std::numeric_limits<std::uint64_t>::max()});

  CHECK(status.failed());
  CHECK(status.domain() == caors::error_domain::serialization);
  CHECK(status.value() == caors::errc::missing_type);
}
