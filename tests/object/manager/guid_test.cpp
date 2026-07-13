// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：object_manager_guid_manager_binds_persistent_object_guids_through_index_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager binds persistent object guids through index", "[object]") {
  caors::objects::object_index index;
  caors::objects::manager manager(index);

  const caors::error actor_status = register_object_test_type<managed_actor>();
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));

  managed_actor* instance = manager.create_with_guid<managed_actor>(test_object_guid_a);
  REQUIRE(instance != nullptr);

  const caors::object_id id = instance->id();
  CHECK(index.find(test_object_guid_a) == id);
  CHECK(index.guid_of(id) == test_object_guid_a);
  CHECK(manager.guid_of(id) == test_object_guid_a);
  CHECK(manager.find(test_object_guid_a) == instance);
  CHECK_FALSE(manager.bind_guid(id, test_object_guid_b));

  CHECK(manager.destroy(id));
  CHECK(index.empty());
  CHECK(manager.find(test_object_guid_a) == nullptr);
}

// 原测试文件：object_manager_guid_manager_clear_unbinds_persistent_object_guids_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager clear unbinds persistent object guids", "[object]") {
  caors::objects::object_index index;
  caors::objects::manager manager(index);

  const caors::error actor_status = register_object_test_type<managed_actor>();
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));
  const caors::error player_status = register_object_test_type<managed_player>();
  REQUIRE((player_status == caors::error{} ||
           player_status == object_error(caors::errc::duplicate_id)));

  managed_actor* actor = manager.create_with_guid<managed_actor>(test_object_guid_a);
  managed_player* player = manager.create_with_guid<managed_player>(test_object_guid_b);
  REQUIRE(actor != nullptr);
  REQUIRE(player != nullptr);
  REQUIRE(index.size() == 2);

  manager.clear();

  CHECK(manager.empty());
  CHECK(index.empty());
  CHECK(manager.find(test_object_guid_a) == nullptr);
  CHECK(manager.find(test_object_guid_b) == nullptr);
}

// 原测试文件：object_manager_guid_manager_rejects_duplicate_persistent_guid_bindings_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager rejects duplicate persistent guid bindings", "[object]") {
  caors::objects::object_index index;
  caors::objects::manager manager(index);

  const caors::error actor_status = register_object_test_type<managed_actor>();
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));

  managed_actor* first = manager.create_with_guid<managed_actor>(test_object_guid_a);
  REQUIRE(first != nullptr);
  const caors::object_id first_id = first->id();

  CHECK(manager.create_with_guid<managed_actor>(test_object_guid_a) == nullptr);
  CHECK(manager.size() == 1);
  CHECK(index.size() == 1);
  CHECK(manager.find(test_object_guid_a) == first);
  CHECK(index.find(test_object_guid_a) == first_id);

  managed_actor* second = manager.create<managed_actor>();
  REQUIRE(second != nullptr);
  CHECK_FALSE(manager.bind_guid(second->id(), test_object_guid_a));
  CHECK(manager.guid_of(second->id()) == caors::object_guid::nil());

  CHECK(manager.destroy(second));
  CHECK(manager.destroy(first_id));
  CHECK(manager.empty());
  CHECK(index.empty());
}

// 原测试文件：object_manager_guid_manager_rejects_persistent_guid_operations_without_index_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager rejects persistent guid operations without index", "[object]") {
  caors::objects::manager manager;

  const caors::error actor_status = register_object_test_type<managed_actor>();
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));

  CHECK(manager.create_with_guid<managed_actor>(test_object_guid_a) == nullptr);
  CHECK(manager.create_with_guid(managed_actor::static_type_id(), test_object_guid_a) == nullptr);

  managed_actor* instance = manager.create<managed_actor>();
  REQUIRE(instance != nullptr);

  CHECK_FALSE(manager.bind_guid(instance->id(), test_object_guid_a));
  CHECK(manager.guid_of(instance->id()) == caors::object_guid::nil());
  CHECK(manager.find(test_object_guid_a) == nullptr);

  CHECK(manager.destroy(instance));
  CHECK(manager.empty());
}
