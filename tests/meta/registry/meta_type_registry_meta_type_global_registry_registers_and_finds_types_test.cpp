// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_type_registry_test_support.h"

using namespace caors::tests::meta_type_registry;
TEST_CASE("meta type - global registry registers and finds types", "[meta]") {
  CHECK(caors::meta::type_<registry_parent_type>("caors.test.type.registry_parent").result() ==
        caors::error{});
  CHECK(caors::meta::type_<registry_parent_type>("caors.test.type.registry_parent").result() ==
        meta_error(caors::errc::duplicate_id));

  auto type = caors::meta::find("caors.test.type.registry_parent");
  REQUIRE(type);
  CHECK(type.id());
  CHECK(type.name() == "caors.test.type.registry_parent");
  CHECK(type.size() == sizeof(registry_parent_type));
  CHECK(type.alignment() == alignof(registry_parent_type));

  auto by_name = caors::meta::find("caors.test.type.registry_parent");
  REQUIRE(by_name);
  CHECK(by_name.id() == type.id());
}
