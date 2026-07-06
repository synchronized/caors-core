// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - ref can hold unresolved guid", "[resources]") {
  caors::resources::ref<texture_resource> reference{test_guid_a};

  CHECK(reference);
  CHECK(reference.has_guid());
  CHECK_FALSE(reference.resolved());
  CHECK(reference.get() == nullptr);
  CHECK(reference.guid() == test_guid_a);

  reference.unresolve();
  CHECK(reference.guid() == test_guid_a);
}

TEST_CASE("resources - ref resolves compatible resource", "[resources]") {
  texture_resource texture{test_guid_a};
  caors::resources::ref<texture_resource> reference{test_guid_a};

  REQUIRE(reference.resolve(&texture));

  CHECK(reference.resolved());
  CHECK(reference.get() == &texture);
  CHECK(reference->guid() == test_guid_a);
  CHECK((*reference).guid() == test_guid_a);

  reference.unresolve();

  CHECK_FALSE(reference.resolved());
  CHECK(reference.guid() == test_guid_a);
}

TEST_CASE("resources - ref rejects mismatched guid", "[resources]") {
  texture_resource texture{test_guid_b};
  caors::resources::ref<texture_resource> reference{test_guid_a};

  CHECK_FALSE(reference.resolve(&texture));
  CHECK_FALSE(reference.resolved());
  CHECK(reference.guid() == test_guid_a);
}

TEST_CASE("resources - ref supports derived to base conversion", "[resources]") {
  image_texture_resource image{test_guid_a};
  caors::resources::ref<image_texture_resource> derived{&image};
  caors::resource_ref<texture_resource> base{derived};

  CHECK(base.resolved());
  CHECK(base.get() == &image);
  CHECK(base.guid() == test_guid_a);

  base.reset();
  CHECK_FALSE(base);
  CHECK_FALSE(base.resolved());
  CHECK(base.guid() == caors::resource_guid::nil());
}
