// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - saver can persist cache entries", "[resources]") {
  caors::resource_cache<texture_resource> cache;
  REQUIRE(cache.create(test_guid_a) != nullptr);

  counting_texture_saver saver;

  caors::error saved = caors::save_from(cache, saver, test_guid_a);
  REQUIRE(saved.succeeded());
  CHECK(saver.save_count == 1);
  CHECK(saver.saved_guid == test_guid_a);

  caors::error invalid = caors::save_from(cache, saver, caors::resource_guid::nil());
  REQUIRE(invalid.failed());
  CHECK(invalid.domain() == caors::error_domain::resources);
  CHECK(invalid.value() == caors::errc::invalid_argument);

  caors::error missing = caors::save_from(cache, saver, test_guid_b);
  REQUIRE(missing.failed());
  CHECK(missing.domain() == caors::error_domain::resources);
  CHECK(missing.value() == caors::errc::not_found);
  CHECK(saver.save_count == 1);
}
