// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_type_builder_test_support.h"

using namespace caors::tests::meta_type_builder;
TEST_CASE("meta type - builder updates registered data in place", "[meta]") {
  const auto guid = caors::guid::parse("2f7cc996-e0bc-4b61-85ea-b0263b876081");
  REQUIRE(guid.has_value());

  auto builder = caors::meta::type_<stable_builder_type>().type("caors.test.type.stable_builder");
  const caors::meta::type before = builder.get();
  REQUIRE(before);
  CHECK_FALSE(before.guid());
  CHECK(before.size() == sizeof(stable_builder_type));
  CHECK(before.alignment() == alignof(stable_builder_type));

  REQUIRE(builder.guid(caors::type_guid{*guid}).result() == caors::error{});
  const caors::meta::type after = builder.get();
  REQUIRE(after);

  CHECK(before.id() == after.id());
  CHECK(after.size() == sizeof(stable_builder_type));
  CHECK(after.alignment() == alignof(stable_builder_type));
  CHECK(before.guid() == caors::type_guid{*guid});
  CHECK(after.guid() == caors::type_guid{*guid});
}
