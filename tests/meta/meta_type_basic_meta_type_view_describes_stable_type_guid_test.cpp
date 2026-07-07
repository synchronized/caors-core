// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_type_basic_test_support.h"

using namespace caors::tests::meta_type_basic;
TEST_CASE("meta type view - describes stable type guid", "[meta]") {
  const auto guid = caors::guid::parse("5ed39a97-7b20-4d87-86ac-e62a4a615b10");
  REQUIRE(guid.has_value());

  const caors::meta::type type = caors::meta::type_<guid_sample_type>("caors.test.guid.sample")
                                     .guid(caors::type_guid{*guid})
                                     .get();

  CHECK(type.valid());
  CHECK(type.id());
  CHECK(type.guid() == caors::type_guid{*guid});
  CHECK(type.guid().value.string() == "5ed39a97-7b20-4d87-86ac-e62a4a615b10");
}
