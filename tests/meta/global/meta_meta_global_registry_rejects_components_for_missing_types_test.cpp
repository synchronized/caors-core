// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_test_support.h"

using namespace caors::tests::meta_registry;
TEST_CASE("meta - global registry rejects components for missing types", "[meta]") {
  CHECK(caors::meta::get_registry().emplace<missing_component_meta>(
            caors::type_id{std::numeric_limits<std::uint64_t>::max()}, true) ==
        meta_error(caors::errc::missing_type));
  CHECK(caors::meta::get_registry().emplace<missing_component_meta>(
            caors::type_id::invalid(), true) == meta_error(caors::errc::invalid_type));
}
