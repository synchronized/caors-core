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
  CHECK(builder.get().get_ops<caors::meta::factory_ops>() == nullptr);
}

// 原测试文件：meta_type_factory_meta_type_factory_registers_custom_construction_strategy_test.cpp

#include "meta_type_factory_test_support.h"

using namespace caors::tests::meta_type_factory;
TEST_CASE("meta type - factory registers custom construction strategy", "[meta]") {
  auto builder = caors::meta::class_<custom_factory_type>("caors.test.type.factory_custom")
                     .factory(&construct_custom_factory_type, &destroy_custom_factory_type);
  CHECK(builder.succeeded());

  const caors::meta::factory_ops* factory = builder.get().get_ops<caors::meta::factory_ops>();
  REQUIRE(factory != nullptr);
  REQUIRE(factory->valid());

  auto* value = static_cast<custom_factory_type*>(factory->construct(nullptr));
  REQUIRE(value != nullptr);
  CHECK(value->value == 23);
  factory->destroy(nullptr, value);

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

  const caors::meta::factory_ops* factory = type.get_ops<caors::meta::factory_ops>();
  REQUIRE(factory != nullptr);
  REQUIRE(factory->valid());

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  auto* value = static_cast<default_factory_type*>(factory->construct(&tracking));
  REQUIRE(value != nullptr);
  CHECK(value->value == 11);
  CHECK(tracking.statistics().allocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use >= sizeof(default_factory_type));

  factory->destroy(&tracking, value);
  CHECK(tracking.statistics().deallocation_count == 1);
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
