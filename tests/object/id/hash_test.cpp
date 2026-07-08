// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - object_id supports standard unordered containers", "[object]") {
  const caors::object_id id = caors::objects::generate_object_id();
  REQUIRE(id);

  std::unordered_map<caors::object_id, int> values;
  values.emplace(id, 7);

  CHECK(values.at(id) == 7);
}
