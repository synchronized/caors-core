// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：object_manager_basic_manager_clear_releases_objects_and_invalidates_handles_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

namespace {

class local_context_managed_actor : public caors::object_base<local_context_managed_actor> {
public:
  static constexpr std::string_view object_type_name =
      "caors.test.object.local_context_managed_actor";
};

} // namespace

TEST_CASE("object - manager clear releases objects and invalidates handles", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  {
    caors::objects::manager manager(&tracking);

    const caors::error actor_status = register_object_test_type<managed_actor>();
    REQUIRE((actor_status == caors::error{} ||
             actor_status == object_error(caors::errc::duplicate_id)));
    const caors::error player_status = register_object_test_type<managed_player>();
    REQUIRE((player_status == caors::error{} ||
             player_status == object_error(caors::errc::duplicate_id)));

    managed_actor* actor = manager.create<managed_actor>();
    managed_player* player = manager.create<managed_player>();
    REQUIRE(actor != nullptr);
    REQUIRE(player != nullptr);

    const caors::objects::handle actor_handle = manager.handle_of(actor->id());
    const caors::objects::handle player_handle = manager.handle_of(player->id());

    REQUIRE(manager.size() == 2);
    REQUIRE(actor_handle);
    REQUIRE(player_handle);

    manager.clear();

    CHECK(manager.empty());
    CHECK_FALSE(manager.alive(actor_handle));
    CHECK_FALSE(manager.alive(player_handle));
    CHECK(manager.find(actor_handle) == nullptr);
    CHECK(manager.find(player_handle) == nullptr);
  }

  CHECK(tracking.statistics().bytes_in_use == 0);
}

// 原测试文件：object_manager_basic_manager_owns_objects_and_resolves_handles_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager owns objects and resolves handles", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());

  {
    caors::objects::manager manager(&tracking);

    const caors::error actor_status = register_object_test_type<managed_actor>();
    REQUIRE((actor_status == caors::error{} ||
             actor_status == object_error(caors::errc::duplicate_id)));

    managed_actor* instance = manager.create<managed_actor>();
    REQUIRE(instance != nullptr);
    instance->value = 13;

    const caors::object_id id = instance->id();
    const caors::objects::handle handle = manager.handle_of(id);

    CHECK(manager.size() == 1);
    CHECK(manager.contains(id));
    CHECK(handle);
    CHECK(handle.id() == id);
    CHECK(handle.type() == managed_actor::static_type_id());
    CHECK(manager.find(id) == instance);
    CHECK(manager.find(handle) == instance);
    CHECK(manager.find<managed_actor>(id) == instance);
    CHECK(manager.find<managed_actor>(handle) == instance);
    CHECK(manager.alive(handle));

    CHECK(manager.destroy(id));
    CHECK(manager.empty());
    CHECK_FALSE(manager.contains(id));
    CHECK_FALSE(manager.alive(handle));
    CHECK(manager.find(id) == nullptr);
    CHECK(manager.find(handle) == nullptr);
    CHECK_FALSE(manager.destroy(id));
  }

  CHECK(tracking.statistics().bytes_in_use == 0);
}

// 原测试文件：object_manager_basic_manager_rejects_unregistered_and_foreign_objects_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager rejects unregistered and foreign objects", "[object]") {
  caors::objects::manager manager;

  CHECK(manager.create<unregistered_managed_actor>() == nullptr);

  const caors::error actor_status = register_object_test_type<managed_actor>();
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));

  managed_actor external{};
  CHECK_FALSE(manager.destroy(&external));
  CHECK_FALSE(manager.contains(external.id()));
  CHECK(manager.find(external.id()) == nullptr);
}

// 原测试文件：object_manager_basic_manager_supports_registered_non_default_constructible_objects_test.cpp

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager supports registered non default constructible objects", "[object]") {
  caors::objects::manager manager;

  REQUIRE(register_object_test_type<managed_non_default_object>() == caors::error{});

  CHECK(manager.create(managed_non_default_object::static_type_id()) == nullptr);

  managed_non_default_object* instance = manager.create<managed_non_default_object>(42);
  REQUIRE(instance != nullptr);
  CHECK(instance->value == 42);
  CHECK(manager.find<managed_non_default_object>(instance->id()) == instance);

  CHECK(manager.destroy(instance));
  CHECK(manager.empty());
}

TEST_CASE("object - manager can use explicit meta type context", "[object]") {
  caors::meta::registry registry;
  caors::meta::context context{registry};

  REQUIRE(caors::registration::type_<local_context_managed_actor>(
              context, local_context_managed_actor::object_type_name)
              .commit() == caors::error{});

  CHECK_FALSE(caors::objects::find<local_context_managed_actor>());

  const auto object_type = caors::objects::find<local_context_managed_actor>(context);
  REQUIRE(object_type);

  caors::objects::manager manager{context};
  caors::object* instance = manager.create(object_type.id());
  REQUIRE(instance != nullptr);
  CHECK(instance->type_id() == object_type.id());
  CHECK(manager.find(instance->id()) == instance);

  CHECK(manager.destroy(instance));
  CHECK(manager.empty());
}
