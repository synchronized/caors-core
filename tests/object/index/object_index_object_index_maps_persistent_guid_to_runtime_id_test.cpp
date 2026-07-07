// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - object index maps persistent guid to runtime id", "[object]") {
  caors::objects::object_index index;
  const caors::object_id first = caors::objects::generate_object_id();
  const caors::object_id second = caors::objects::generate_object_id();

  REQUIRE(first);
  REQUIRE(second);
  REQUIRE(first != second);

  CHECK(index.bind(test_object_guid_a, first));
  CHECK(index.bind(test_object_guid_a, first));
  CHECK(index.size() == 1);
  CHECK(index.contains(test_object_guid_a));
  CHECK(index.contains(first));
  CHECK(index.find(test_object_guid_a) == first);
  CHECK(index.guid_of(first) == test_object_guid_a);

  CHECK_FALSE(index.bind(test_object_guid_a, second));
  CHECK_FALSE(index.bind(test_object_guid_b, first));
  CHECK_FALSE(index.bind(caors::object_guid::nil(), second));
  CHECK_FALSE(index.bind(test_object_guid_b, caors::object_id::invalid()));

  CHECK(index.unbind(first));
  CHECK(index.empty());
  CHECK_FALSE(index.contains(test_object_guid_a));
  CHECK(index.find(test_object_guid_a) == caors::object_id::invalid());
  CHECK(index.guid_of(first) == caors::object_guid::nil());
}
