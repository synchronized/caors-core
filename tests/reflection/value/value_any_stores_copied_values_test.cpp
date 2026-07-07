// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any - stores copied values", "[reflection]") {
  require_value_test_meta();

  int source = 7;
  caors::any stored = caors::any::from(source);

  CHECK(stored.valid());
  const auto int_type = caors::meta::find("int");
  REQUIRE(int_type);
  CHECK(stored.type() == int_type.id());
  CHECK(stored.type_name() == "int");
  CHECK(stored.matches_type<int>());
  CHECK(stored.size() == sizeof(int));
  CHECK(stored.alignment() == alignof(int));

  source = 8;
  const int* read = stored.try_get<int>();
  REQUIRE(read != nullptr);
  CHECK(*read == 7);

  int* write = stored.try_get_mutable<int>();
  REQUIRE(write != nullptr);
  *write = 9;
  CHECK(source == 8);
  CHECK(*stored.try_get<int>() == 9);

  CHECK(stored.try_set(10));
  CHECK(*stored.try_get<int>() == 10);
}
