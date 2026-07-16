// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：meta_type_factory_meta_type_default_factory_rejects_non_default_constructible_types_test.cpp

#include "meta_type_factory_test_support.h"

using namespace caors::tests::meta_type_factory;
TEST_CASE("meta type - default factory rejects non-default-constructible types", "[meta]") {
  auto builder =
      caors::meta::class_<missing_default_factory_type>("caors.test.type.factory_missing_default")
          .factory();

  CHECK(builder.result() == meta_error(caors::errc::invalid_type));
  REQUIRE(builder.get().valid());
  CHECK_FALSE(builder.get().get_capability<caors::meta::factory_capability>());
}

// 原测试文件：meta_type_factory_meta_type_factory_registers_custom_construction_strategy_test.cpp

#include "meta_type_factory_test_support.h"

using namespace caors::tests::meta_type_factory;
TEST_CASE("meta type - factory registers custom construction strategy", "[meta]") {
  auto builder = caors::meta::class_<custom_factory_type>("caors.test.type.factory_custom")
                     .factory(&construct_custom_factory_type, &destroy_custom_factory_type);
  CHECK(builder.succeeded());

  const caors::meta::factory_capability factory =
      builder.get().get_capability<caors::meta::factory_capability>();
  REQUIRE(factory);
  REQUIRE(factory.valid());

  caors::owned_value constructed = factory.construct_owned();
  REQUIRE(constructed.valid());
  auto* constructed_value = constructed.ref().try_get_mutable<custom_factory_type>();
  REQUIRE(constructed_value != nullptr);
  CHECK(constructed_value->value == 23);

  caors::any moved = std::move(constructed).into_any();
  REQUIRE(moved.valid());
  CHECK_FALSE(constructed.valid());
  auto* moved_value = moved.try_get_mutable<custom_factory_type>();
  REQUIRE(moved_value != nullptr);
  CHECK(moved_value->value == 23);

  caors::any owned = factory.construct();
  REQUIRE(owned.valid());
  auto* owned_value = owned.try_get_mutable<custom_factory_type>();
  REQUIRE(owned_value != nullptr);
  CHECK(owned_value->value == 23);

  auto duplicate = caors::meta::class_<custom_factory_type>("caors.test.type.factory_custom")
                       .factory(&construct_custom_factory_type, &destroy_custom_factory_type);
  CHECK(duplicate.result() == meta_error(caors::errc::duplicate_id));
  CHECK(duplicate.succeeded());
}

// 原测试文件：meta_type_factory_meta_type_factory_registers_resource_aware_default_construction_test.cpp

#include "meta_type_factory_test_support.h"

using namespace caors::tests::meta_type_factory;
TEST_CASE("meta type - factory registers resource-aware default construction", "[meta]") {
  auto builder =
      caors::meta::class_<default_factory_type>("caors.test.type.factory_default").factory();
  CHECK(builder.succeeded());

  const caors::meta::type type = builder.get();
  REQUIRE(type.valid());

  const caors::meta::factory_capability factory =
      type.get_capability<caors::meta::factory_capability>();
  REQUIRE(factory);
  REQUIRE(factory.valid());

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::owned_value constructed = factory.construct_owned(&tracking);
  REQUIRE(constructed.valid());
  auto* constructed_value = constructed.ref().try_get_mutable<default_factory_type>();
  REQUIRE(constructed_value != nullptr);
  CHECK(constructed_value->value == 11);
  CHECK(tracking.statistics().allocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use >= sizeof(default_factory_type));

  constructed.reset();
  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);

  caors::any owned = factory.construct(&tracking);
  REQUIRE(owned.valid());
  auto* owned_value = owned.try_get_mutable<default_factory_type>();
  REQUIRE(owned_value != nullptr);
  CHECK(owned_value->value == 11);
  CHECK(tracking.statistics().allocation_count == 2);
  CHECK(tracking.statistics().bytes_in_use >= sizeof(default_factory_type));

  owned.reset();
  CHECK(tracking.statistics().deallocation_count == 2);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

// 原测试文件：meta_type_factory_meta_type_resource_default_construction_propagates_allocator_test.cpp

#include "meta_type_factory_test_support.h"

using namespace caors::tests::meta_type_factory;
TEST_CASE("meta type - resource default construction propagates allocator", "[meta]") {
  using value_type = caors::vector<int>;

  const caors::meta::type type =
      caors::meta::class_<value_type>("caors.test.type.resource_vector").get();
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
