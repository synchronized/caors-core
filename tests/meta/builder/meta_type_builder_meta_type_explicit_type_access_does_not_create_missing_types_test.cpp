// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_type_builder_test_support.h"

using namespace caors::tests::meta_type_builder;
TEST_CASE("meta type - explicit type access does not create missing types", "[meta]") {
  CHECK_FALSE(caors::meta::find<missing_find_type>());

  const caors::meta::type type =
      caors::meta::type_<explicit_find_type>("caors.test.type.explicit_find").get();
  REQUIRE(type.valid());

  const caors::meta::type found = caors::meta::find<explicit_find_type>();
  REQUIRE(found.valid());
  CHECK(found.id() == type.id());
}
