// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_type_factory_test_support.h"

using namespace caors::tests::meta_type_factory;
TEST_CASE("meta type - default factory rejects non-default-constructible types", "[meta]") {
  auto builder =
      caors::meta::type_<missing_default_factory_type>("caors.test.type.factory_missing_default")
          .factory();

  CHECK(builder.result() == meta_error(caors::errc::invalid_type));
  REQUIRE(builder.get().valid());
  CHECK_FALSE(caors::meta::get_registry().find_type_factory(builder.get().id()).valid());
}
