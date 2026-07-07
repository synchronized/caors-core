// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

namespace {

class weak_counted_resource : public caors::resource {
public:
  using caors::resource::resource;

  ~weak_counted_resource() override { ++destroyed; }

  inline static int destroyed = 0;
};

} // namespace

TEST_CASE("resources - weak handles observe resource lifetime", "[resources]") {
  weak_counted_resource::destroyed = 0;

  caors::resource_handle<weak_counted_resource> strong =
      caors::make_handle<weak_counted_resource>(test_guid_a);
  caors::resource_weak_handle<weak_counted_resource> weak = strong;

  CHECK_FALSE(weak.expired());
  CHECK(weak.use_count() == 1);

  caors::resource_handle<weak_counted_resource> locked = weak.lock();
  REQUIRE(locked);
  CHECK(locked.get() == strong.get());
  CHECK(strong.use_count() == 2);

  locked.reset();
  strong.reset();

  CHECK(weak.expired());
  CHECK(weak.lock() == nullptr);
  CHECK(weak_counted_resource::destroyed == 1);
}

TEST_CASE("resources - handles convert from derived to base resource types", "[resources]") {
  caors::resource_handle<image_texture_resource> derived =
      caors::make_handle<image_texture_resource>(test_guid_a);
  REQUIRE(derived);

  caors::resource_handle<texture_resource> base = derived;
  CHECK(base.get() == derived.get());
  CHECK(base.use_count() == 2);

  caors::resource_weak_handle<texture_resource> weak = derived;
  caors::resource_handle<texture_resource> locked = weak.lock();
  REQUIRE(locked);
  CHECK(locked.get() == derived.get());
  CHECK(derived.use_count() == 3);
}
