// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - resource guid wraps stable guid", "[resources]") {
  CHECK(caors::resource_guid::nil().is_nil());
  CHECK_FALSE(static_cast<bool>(caors::resource_guid::nil()));
  CHECK(test_guid_a);
  CHECK(test_guid_a != test_guid_b);

  const auto parsed = caors::resource_guid::parse(test_guid_a.string());
  REQUIRE(parsed.has_value());
  CHECK(*parsed == test_guid_a);
  CHECK_FALSE(caors::resource_guid::parse("not-a-guid").has_value());

  caors::hash_map<caors::resource_guid, int, caors::resource_guid_hash> values;
  values.emplace(test_guid_a, 7);
  REQUIRE(values.contains(test_guid_a));
  CHECK(values[test_guid_a] == 7);
}
