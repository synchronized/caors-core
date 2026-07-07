// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - type guid can be provided by object subclass", "[object]") {
  CHECK(persistent_actor::static_type_guid() == persistent_actor::object_type_guid);

  REQUIRE(register_object_test_type<persistent_actor>() == caors::error{});

  auto stored = caors::objects::find<persistent_actor>();
  REQUIRE(stored);
  CHECK(stored.guid() == persistent_actor::object_type_guid);
}
