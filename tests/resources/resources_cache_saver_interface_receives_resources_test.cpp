// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - saver interface receives resources", "[resources]") {
  texture_resource texture{test_guid_a};
  counting_texture_saver saver;

  const caors::error status = saver.save(texture);

  REQUIRE(status.succeeded());
  CHECK(saver.save_count == 1);
  CHECK(saver.saved_guid == test_guid_a);
}
