// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：object_index_object_index_maps_persistent_guid_to_runtime_id_test.cpp

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

// 原测试文件：object_index_object_index_unbinds_by_guid_and_clears_mappings_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - object index unbinds by guid and clears mappings", "[object]") {
  caors::objects::object_index index;
  const caors::object_id first = caors::objects::generate_object_id();
  const caors::object_id second = caors::objects::generate_object_id();

  REQUIRE(index.bind(test_object_guid_a, first));
  REQUIRE(index.bind(test_object_guid_b, second));
  REQUIRE(index.size() == 2);

  CHECK(index.unbind(test_object_guid_a));
  CHECK_FALSE(index.unbind(test_object_guid_a));
  CHECK_FALSE(index.contains(test_object_guid_a));
  CHECK_FALSE(index.contains(first));
  CHECK(index.find(test_object_guid_a) == caors::object_id::invalid());
  CHECK(index.guid_of(first) == caors::object_guid::nil());
  CHECK(index.find(test_object_guid_b) == second);
  CHECK(index.guid_of(second) == test_object_guid_b);

  index.clear();

  CHECK(index.empty());
  CHECK_FALSE(index.contains(test_object_guid_b));
  CHECK_FALSE(index.contains(caors::object_guid::nil()));
  CHECK_FALSE(index.contains(caors::object_id::invalid()));
  CHECK(index.find(test_object_guid_b) == caors::object_id::invalid());
  CHECK(index.guid_of(second) == caors::object_guid::nil());
}
