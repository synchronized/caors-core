// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：meta_type_factory_meta_type_default_factory_rejects_non_default_constructible_types_test.cpp

#include "meta_type_factory_test_support.h"

using namespace caors::tests::meta_type_factory;
TEST_CASE("meta type - default factory rejects non-default-constructible types", "[meta]") {
  auto builder = caors::registration::type_<missing_default_factory_type>(
      "caors.test.type.factory_missing_default");
  builder.meta().factory();

  CHECK(builder.commit() == meta_error(caors::errc::invalid_type));
  CHECK_FALSE(caors::meta::find<missing_default_factory_type>());
}

// 原测试文件：meta_type_factory_meta_type_factory_registers_custom_construction_strategy_test.cpp

#include "meta_type_factory_test_support.h"

using namespace caors::tests::meta_type_factory;
TEST_CASE("meta type - factory registers custom construction strategy", "[meta]") {
  auto builder = caors::registration::type_<custom_factory_type>("caors.test.type.factory_custom");
  builder.meta().factory(&construct_custom_factory_type, &destroy_custom_factory_type);
  REQUIRE(builder.commit() == caors::error{});

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

  auto duplicate =
      caors::registration::type_<custom_factory_type>("caors.test.type.factory_custom");
  duplicate.meta().factory(&construct_custom_factory_type, &destroy_custom_factory_type);
  CHECK(duplicate.commit() == meta_error(caors::errc::duplicate_id));
}

TEST_CASE("meta type - owned value release exposes manual destruction", "[meta]") {
  const caors::meta::type type = caors::meta::resolve<custom_factory_type>();
  auto* raw = new custom_factory_type(31);
  caors::owned_value value = caors::meta::make_owned_value(type, raw, [](void* instance) noexcept {
    delete static_cast<custom_factory_type*>(instance);
  });

  caors::meta::released_owned_value released = std::move(value).release();

  REQUIRE(released.valid());
  CHECK_FALSE(value.valid());
  CHECK(released.type.id() == type.id());
  CHECK(released.value == raw);
  CHECK(released.resource == nullptr);
  CHECK(released.resource_destroy == nullptr);
  REQUIRE(released.destroy != nullptr);

  released.destroy(released.value);
}

TEST_CASE("meta type - owned value into_any preserves custom destroy function", "[meta]") {
  owned_value_custom_destroy_type::reset_counts();

  const caors::meta::type type = caors::meta::resolve<owned_value_custom_destroy_type>();
  auto* raw = new owned_value_custom_destroy_type(41);
  caors::owned_value value =
      caors::meta::make_owned_value(type, raw, &destroy_owned_value_custom_destroy_type);

  caors::any owned = std::move(value).into_any();
  REQUIRE(owned.valid());
  CHECK_FALSE(value.valid());
  auto* stored = owned.try_get_mutable<owned_value_custom_destroy_type>();
  REQUIRE(stored != nullptr);
  CHECK(stored->value == 41);

  owned.reset();

  CHECK(owned_value_custom_destroy_type::custom_destroy_count == 1);
  CHECK(owned_value_custom_destroy_type::destructor_count == 1);
}

TEST_CASE("meta type - released owned value guard destroys on scope exit", "[meta]") {
  const caors::meta::type type = caors::meta::resolve<default_factory_type>();
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  auto* raw = caors::memory::make<default_factory_type>(&tracking);
  caors::owned_value value = caors::meta::make_owned_value(
      type, raw, &tracking, [](caors::memory_resource* resource, void* instance) noexcept {
        caors::memory::destroy(static_cast<default_factory_type*>(instance), resource);
      });

  {
    caors::meta::released_owned_value_guard guard{std::move(value).release()};
    REQUIRE(guard.valid());
    CHECK(guard.data() == raw);
    CHECK(guard.resource() == &tracking);
    CHECK(tracking.statistics().deallocation_count == 0);
  }

  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("meta type - released owned value guard can release responsibility", "[meta]") {
  const caors::meta::type type = caors::meta::resolve<default_factory_type>();
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  auto* raw = caors::memory::make<default_factory_type>(&tracking);
  caors::owned_value value = caors::meta::make_owned_value(
      type, raw, &tracking, [](caors::memory_resource* resource, void* instance) noexcept {
        caors::memory::destroy(static_cast<default_factory_type*>(instance), resource);
      });

  caors::meta::released_owned_value released{};
  {
    caors::meta::released_owned_value_guard guard{std::move(value).release()};
    released = guard.release();
    CHECK_FALSE(guard.valid());
  }

  CHECK(tracking.statistics().deallocation_count == 0);
  REQUIRE(released.valid());
  released.resource_destroy(released.resource, released.value);
  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

// 原测试文件：meta_type_factory_meta_type_factory_registers_resource_aware_default_construction_test.cpp

#include "meta_type_factory_test_support.h"

using namespace caors::tests::meta_type_factory;
TEST_CASE("meta type - factory registers resource-aware default construction", "[meta]") {
  auto builder =
      caors::registration::type_<default_factory_type>("caors.test.type.factory_default");
  builder.meta().factory();
  REQUIRE(builder.commit() == caors::error{});

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

  caors::meta::released_owned_value released = std::move(constructed).release();
  REQUIRE(released.valid());
  CHECK_FALSE(constructed.valid());
  CHECK(released.type.id() == type.id());
  CHECK(released.resource == &tracking);
  REQUIRE(released.resource_destroy != nullptr);
  CHECK(released.destroy == nullptr);

  released.resource_destroy(released.resource, released.value);
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

  const caors::meta::type static_type = caors::meta::resolve<value_type>();
  REQUIRE(static_type);
  REQUIRE_FALSE(static_type.name().empty());
  REQUIRE(caors::registration::type_<value_type>(static_type.name().view()).commit() ==
          caors::error{});
  const caors::meta::type type = caors::meta::find<value_type>();
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
