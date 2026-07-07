// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_type_registry_test_support.h"

using namespace caors::tests::meta_type_registry;
TEST_CASE("meta type - global registry checks parent relationship", "[meta]") {
  REQUIRE(caors::meta::type_<relation_parent_type>("caors.test.type.relation_parent").result() ==
          caors::error{});
  const auto parent = caors::meta::find("caors.test.type.relation_parent");
  REQUIRE(parent);

  REQUIRE(caors::meta::type_<relation_child_type>("caors.test.type.relation_child")
              .parent(parent.id())
              .result() == caors::error{});
  const auto child = caors::meta::find("caors.test.type.relation_child");
  REQUIRE(child);

  CHECK(caors::meta::get_registry().contains(parent.id()));
  CHECK(caors::meta::get_registry().contains(child.id()));
  CHECK(caors::meta::is_a(child.id(), child.id()));
  CHECK(caors::meta::is_a(child.id(), parent.id()));
  CHECK_FALSE(caors::meta::is_a(parent.id(), child.id()));
  CHECK(child.parent().id() == parent.id());
}
