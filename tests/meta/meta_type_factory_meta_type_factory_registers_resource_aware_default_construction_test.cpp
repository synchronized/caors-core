// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_type_factory_test_support.h"

using namespace caors::tests::meta_type_factory;
TEST_CASE("meta type - factory registers resource-aware default construction", "[meta]") {
  auto builder =
      caors::meta::type_<default_factory_type>("caors.test.type.factory_default").factory();
  CHECK(builder.succeeded());

  const caors::meta::type type = builder.get();
  REQUIRE(type.valid());

  const caors::meta::detail::value_factory_ops factory =
      caors::meta::get_registry().find_type_factory(type.id());
  REQUIRE(factory.valid());

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  auto* value = static_cast<default_factory_type*>(factory.construct(&tracking));
  REQUIRE(value != nullptr);
  CHECK(value->value == 11);
  CHECK(tracking.statistics().allocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use >= sizeof(default_factory_type));

  factory.destroy(&tracking, value);
  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}
