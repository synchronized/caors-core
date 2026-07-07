// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_type_factory_test_support.h"

using namespace caors::tests::meta_type_factory;
TEST_CASE("meta type - resource default construction propagates allocator", "[meta]") {
  using value_type = caors::vector<int>;

  const caors::meta::type type =
      caors::meta::type_<value_type>("caors.test.type.resource_vector").get();
  REQUIRE(type.valid());

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::any value = caors::any::default_construct(type.id(), &tracking);

  REQUIRE(value.valid());
  auto* values = value.try_get_mutable<value_type>();
  REQUIRE(values != nullptr);
  CHECK(values->get_allocator().resource() == &tracking);

  values->reserve(4);
  values->push_back(7);

  CHECK(values->front() == 7);
  CHECK(tracking.statistics().allocation_count >= 2);

  value.reset();

  CHECK(tracking.statistics().bytes_in_use == 0);
  CHECK(tracking.statistics().allocation_count == tracking.statistics().deallocation_count);
}
