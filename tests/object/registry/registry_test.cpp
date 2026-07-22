// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：object_registry_direct_creation_supports_constructor_arguments_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - direct creation supports constructor arguments", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  auto* instance = caors::objects::create<direct_non_default_object>(&tracking, 42);

  REQUIRE(instance != nullptr);
  CHECK(instance->value == 42);
  CHECK(instance->type_id() == direct_non_default_object::static_type_id());

  caors::objects::destroy(instance, &tracking);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

// 原测试文件：object_registry_non_default_constructible_types_can_be_described_but_not_factory_created_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - non default constructible types can be described but not factory-created",
          "[object]") {
  REQUIRE(register_object_test_type<registered_non_default_object>() == caors::error{});

  auto type = caors::objects::find<registered_non_default_object>();
  REQUIRE(type);
  CHECK(caors::objects::contains(type));
  CHECK_FALSE(caors::objects::constructible(type));
  CHECK(caors::objects::try_create(type) == nullptr);
  CHECK(caors::objects::create(type) == nullptr);
}

// 原测试文件：object_registry_registry_checks_parent_relationship_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - registry checks parent relationship", "[object]") {
  REQUIRE(register_object_test_type<relation_actor>() == caors::error{});
  const auto object_type = caors::objects::find<caors::object>();
  const auto actor_type = caors::objects::find<relation_actor>();
  REQUIRE(object_type);
  REQUIRE(actor_type);

  REQUIRE(register_object_test_type<relation_player>(actor_type.id()) == caors::error{});
  const auto player_type = caors::objects::find<relation_player>();
  REQUIRE(player_type);

  CHECK(caors::objects::is_a(player_type.id(), player_type.id()));
  CHECK(caors::objects::is_a(player_type.id(), actor_type.id()));
  CHECK(caors::objects::is_a(player_type.id(), object_type.id()));
  CHECK_FALSE(caors::objects::is_a(actor_type.id(), player_type.id()));
}

// 原测试文件：object_registry_registry_creates_and_destroys_registered_objects_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - registry creates and destroys registered objects", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  REQUIRE(register_object_test_type<factory_actor>() == caors::error{});

  const auto actor_type = caors::objects::find<factory_actor>();
  REQUIRE(actor_type);

  caors::object* instance = caors::objects::create(actor_type, &tracking);

  REQUIRE(instance != nullptr);
  CHECK(instance->type_id() == actor_type.id());
  CHECK(tracking.statistics().allocation_count == 1);

  CHECK(caors::objects::destroy(actor_type, instance, &tracking));
  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

// 原测试文件：object_registry_registry_registers_and_finds_types_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - registry registers and finds types", "[object]") {
  CHECK(register_object_test_type<registry_actor>() == caors::error{});
  CHECK(register_object_test_type<registry_actor>() == object_error(caors::errc::duplicate_id));

  auto type = caors::objects::find<registry_actor>();
  REQUIRE(type);
  CHECK(caors::objects::contains(type));
  CHECK(type.id() == registry_actor::static_type_id());
  CHECK(type.name() == registry_actor::object_type_name);
  CHECK_FALSE(type.guid());
  CHECK(type.size() == sizeof(registry_actor));
  CHECK(type.alignment() == alignof(registry_actor));
  CHECK(caors::objects::constructible(type));

  auto by_name = caors::objects::find(registry_actor::object_type_name);
  REQUIRE(by_name);
  CHECK(by_name.id() == registry_actor::static_type_id());
}

// 原测试文件：object_registry_registry_reports_duplicate_type_guid_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - registry reports duplicate type guid", "[object]") {
  REQUIRE(register_object_test_type<guid_owner_actor>() == caors::error{});

  CHECK(register_object_test_type<duplicate_guid_player>() ==
        object_error(caors::errc::duplicate_guid));
}

// 原测试文件：object_registry_registry_reports_registration_errors_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - registry reports registration errors", "[object]") {
  CHECK(caors::rethrow_as(
            caors::registration::type_<error_actor>("caors.test.object.missing_parent_child")
                .parent(caors::type_id{std::numeric_limits<std::uint64_t>::max()})
                .commit(),
            caors::error_domain::object) == object_error(caors::errc::missing_parent));

  REQUIRE(register_object_test_type<error_actor>() == caors::error{});

  CHECK(register_object_test_type<error_player>(error_actor::object_type_name) ==
        object_error(caors::errc::duplicate_name));
}

// 原测试文件：object_registry_type_guid_can_be_provided_by_object_subclass_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - type guid can be provided by object subclass", "[object]") {
  CHECK(persistent_actor::static_type_guid() == persistent_actor::object_type_guid);

  REQUIRE(register_object_test_type<persistent_actor>() == caors::error{});

  auto stored = caors::objects::find<persistent_actor>();
  REQUIRE(stored);
  CHECK(stored.guid() == persistent_actor::object_type_guid);
}
