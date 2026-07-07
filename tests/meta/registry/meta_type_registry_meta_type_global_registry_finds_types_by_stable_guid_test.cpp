// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_type_registry_test_support.h"

using namespace caors::tests::meta_type_registry;
TEST_CASE("meta type - global registry finds types by stable guid", "[meta]") {
  const auto guid = caors::guid::parse("7b458821-d56a-4133-a5a6-90a8661ee474");
  REQUIRE(guid.has_value());

  REQUIRE(caors::meta::type_<guid_parent_type>("caors.test.type.guid_parent")
              .guid(caors::type_guid{*guid})
              .result() == caors::error{});

  auto type = caors::meta::find(caors::type_guid{*guid});
  REQUIRE(type);
  CHECK(type.name() == "caors.test.type.guid_parent");
  CHECK(type.guid() == caors::type_guid{*guid});
}
