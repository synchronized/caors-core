// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_type_builder_test_support.h"

using namespace caors::tests::meta_type_builder;
TEST_CASE("meta type - builder without name does not register type", "[meta]") {
  const auto builder = caors::meta::type_<lazy_meta_builder_type>();

  CHECK_FALSE(builder.get());
  CHECK_FALSE(caors::meta::find<lazy_meta_builder_type>());
  CHECK_FALSE(caors::meta::find<lazy_meta_builder_type>());
}
