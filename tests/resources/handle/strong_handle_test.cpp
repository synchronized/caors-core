// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

namespace {

class counted_resource : public caors::resource {
public:
  using caors::resource::resource;

  ~counted_resource() override { ++destroyed; }

  int value = 0;
  inline static int destroyed = 0;
};

} // namespace

TEST_CASE("resources - strong handles own resources", "[resources]") {
  counted_resource::destroyed = 0;

  caors::resource_handle<counted_resource> resource =
      caors::make_handle<counted_resource>(test_guid_a);
  REQUIRE(resource);
  CHECK(resource.guid() == test_guid_a);
  CHECK(resource.use_count() == 1);
  CHECK(resource.unique());

  resource->value = 42;

  {
    caors::resource_handle<counted_resource> copy = resource;
    CHECK(copy.get() == resource.get());
    CHECK(copy.use_count() == 2);
    CHECK_FALSE(resource.unique());
    CHECK(copy->value == 42);
  }

  CHECK(resource.use_count() == 1);
  CHECK(counted_resource::destroyed == 0);

  resource.reset();
  CHECK(counted_resource::destroyed == 1);
}

TEST_CASE("resources - handles can use selected memory resources", "[resources]") {
  counted_resource::destroyed = 0;

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  {
    caors::resource_handle<counted_resource> resource =
        caors::make_handle<counted_resource>(&tracking, test_guid_a);
    REQUIRE(resource);
    CHECK(resource.use_count() == 1);
    CHECK(tracking.statistics().bytes_in_use >= sizeof(counted_resource));
  }

  CHECK(counted_resource::destroyed == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}
