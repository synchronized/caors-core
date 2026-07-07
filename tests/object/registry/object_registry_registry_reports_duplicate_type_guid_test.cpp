// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - registry reports duplicate type guid", "[object]") {
  REQUIRE(register_object_test_type<guid_owner_actor>() == caors::error{});

  CHECK(register_object_test_type<duplicate_guid_player>() ==
        object_error(caors::errc::duplicate_guid));
}
