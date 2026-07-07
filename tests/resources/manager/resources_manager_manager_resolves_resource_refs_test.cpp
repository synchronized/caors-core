// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manager resolves resource refs", "[resources]") {
  caors::resource_manager manager;
  image_texture_resource image{test_guid_a};
  texture_resource other{test_guid_b};

  REQUIRE(manager.bind(&image));
  REQUIRE(manager.bind(&other));

  caors::resource_ref<texture_resource> base_ref{test_guid_a};
  REQUIRE(manager.resolve(base_ref));

  CHECK(base_ref.resolved());
  CHECK(base_ref.get() == &image);
  CHECK(base_ref.guid() == test_guid_a);

  caors::resources::ref<image_texture_resource> derived_ref{test_guid_a};
  REQUIRE(manager.resolve(derived_ref));
  CHECK(derived_ref.get() == &image);

  caors::resources::ref<image_texture_resource> mismatched_type{test_guid_b};
  CHECK_FALSE(manager.resolve(mismatched_type));
  CHECK_FALSE(mismatched_type.resolved());
  CHECK(mismatched_type.guid() == test_guid_b);

  caors::resources::ref<texture_resource> missing{caors::resource_guid::nil()};
  CHECK_FALSE(manager.resolve(missing));
}
