// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

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

TEST_CASE("object - manager resolves typed object handles and inheritance", "[object]") {
  caors::objects::manager manager;

  const caors::error actor_status = register_object_test_type<managed_actor>();
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));
  const caors::error player_status = register_object_test_type<managed_player>();
  REQUIRE((player_status == caors::error{} ||
           player_status == object_error(caors::errc::duplicate_id)));

  managed_player* player = manager.create<managed_player>();
  REQUIRE(player != nullptr);

  const caors::object_id id = player->id();
  const caors::objects::typed_handle<managed_player> player_typed =
      manager.handle_of<managed_player>(id);
  const caors::objects::typed_handle<managed_actor> actor_typed =
      manager.handle_of<managed_actor>(id);

  REQUIRE(player_typed);
  REQUIRE(actor_typed);
  CHECK(manager.find(player_typed) == player);
  CHECK(manager.find(actor_typed) == static_cast<managed_actor*>(player));
  CHECK(manager.find(caors::objects::handle{id, managed_actor::static_type_id()}) == player);

  CHECK(manager.destroy(player));
  CHECK(manager.find(player_typed) == nullptr);
  CHECK(manager.find(actor_typed) == nullptr);
  CHECK_FALSE(manager.alive(player_typed.handle()));
}

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

TEST_CASE("object - manager rejects mismatched typed lookups", "[object]") {
  caors::objects::manager manager;

  const caors::error actor_status = register_object_test_type<managed_actor>();
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));
  REQUIRE(register_object_test_type<managed_other_actor>() == caors::error{});

  managed_actor* instance = manager.create<managed_actor>();
  REQUIRE(instance != nullptr);

  CHECK(manager.find<managed_other_actor>(instance->id()) == nullptr);
  CHECK_FALSE(manager.handle_of<managed_other_actor>(instance->id()));
  CHECK(manager.find(caors::objects::handle{instance->id(),
                                            managed_other_actor::static_type_id()}) == nullptr);
}

TEST_CASE("object - manager removes lookup entry before destruction", "[object]") {
  caors::objects::manager manager;

  const caors::error register_status = register_object_test_type<destruction_observer>();
  REQUIRE((register_status == caors::error{} ||
           register_status == object_error(caors::errc::duplicate_id)));

  destruction_observer::manager = &manager;
  destruction_observer::invisible_during_destruction = false;

  destruction_observer* instance = manager.create<destruction_observer>();
  REQUIRE(instance != nullptr);

  const caors::object_id id = instance->id();
  REQUIRE(manager.find(id) == instance);

  CHECK(manager.destroy(id));
  CHECK(destruction_observer::invisible_during_destruction);
  CHECK(manager.find(id) == nullptr);

  destruction_observer::manager = nullptr;
}

TEST_CASE("object - manager destroy_later invalidates lookup before releasing object", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  delayed_destroy_actor::destroyed_count = 0;

  {
    caors::objects::object_index index(&tracking);
    caors::objects::manager manager(index, &tracking);

    const caors::error register_status = register_object_test_type<delayed_destroy_actor>();
    REQUIRE((register_status == caors::error{} ||
             register_status == object_error(caors::errc::duplicate_id)));

    delayed_destroy_actor* instance =
        manager.create_with_guid<delayed_destroy_actor>(test_object_guid_a);
    REQUIRE(instance != nullptr);
    instance->value = 24;

    const caors::object_id id = instance->id();
    const caors::objects::handle handle = manager.handle_of(id);
    const caors::objects::ptr<delayed_destroy_actor> ptr{instance};

    REQUIRE(handle);
    REQUIRE(ptr);
    REQUIRE(index.find(test_object_guid_a) == id);

    CHECK(manager.destroy_later(id));
    CHECK(manager.empty());
    CHECK(manager.pending_destroy_count() == 1);
    CHECK_FALSE(manager.contains(id));
    CHECK_FALSE(manager.alive(handle));
    CHECK(manager.find(id) == nullptr);
    CHECK(manager.find(handle) == nullptr);
    CHECK(index.empty());
    CHECK(index.find(test_object_guid_a) == caors::object_id::invalid());
    CHECK(delayed_destroy_actor::destroyed_count == 0);
    CHECK(ptr.id() == id);
    CHECK(instance->value == 24);

    manager.flush_destroy_queue();

    CHECK(manager.pending_destroy_count() == 0);
    CHECK(delayed_destroy_actor::destroyed_count == 1);
    CHECK_FALSE(manager.destroy_later(id));
  }

  const caors::memory_statistics statistics = tracking.statistics();
  CHECK(statistics.bytes_in_use == 0);
  CHECK(statistics.allocation_count == statistics.deallocation_count);
}

TEST_CASE("object - manager rejects duplicate delayed destruction requests", "[object]") {
  caors::objects::manager manager;
  delayed_destroy_actor::destroyed_count = 0;

  const caors::error register_status = register_object_test_type<delayed_destroy_actor>();
  REQUIRE((register_status == caors::error{} ||
           register_status == object_error(caors::errc::duplicate_id)));

  delayed_destroy_actor* instance = manager.create<delayed_destroy_actor>();
  REQUIRE(instance != nullptr);

  const caors::object_id id = instance->id();
  CHECK(manager.destroy_later(instance));
  CHECK_FALSE(manager.destroy_later(instance));
  CHECK_FALSE(manager.destroy_later(id));
  CHECK_FALSE(manager.destroy(instance));
  CHECK(manager.pending_destroy_count() == 1);
  CHECK(delayed_destroy_actor::destroyed_count == 0);

  manager.flush_destroy_queue();

  CHECK(delayed_destroy_actor::destroyed_count == 1);
  CHECK(manager.pending_destroy_count() == 0);
}

TEST_CASE("object - manager clear releases live and delayed objects", "[object]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  delayed_destroy_actor::destroyed_count = 0;

  {
    caors::objects::object_index index(&tracking);
    caors::objects::manager manager(index, &tracking);

    const caors::error register_status = register_object_test_type<delayed_destroy_actor>();
    REQUIRE((register_status == caors::error{} ||
             register_status == object_error(caors::errc::duplicate_id)));

    delayed_destroy_actor* delayed =
        manager.create_with_guid<delayed_destroy_actor>(test_object_guid_a);
    delayed_destroy_actor* live =
        manager.create_with_guid<delayed_destroy_actor>(test_object_guid_b);
    REQUIRE(delayed != nullptr);
    REQUIRE(live != nullptr);
    REQUIRE(manager.size() == 2);
    REQUIRE(index.size() == 2);

    CHECK(manager.destroy_later(delayed));
    CHECK(manager.size() == 1);
    CHECK(manager.pending_destroy_count() == 1);
    CHECK(index.find(test_object_guid_a) == caors::object_id::invalid());
    CHECK(index.find(test_object_guid_b) == live->id());
    CHECK(delayed_destroy_actor::destroyed_count == 0);

    manager.clear();

    CHECK(manager.empty());
    CHECK(manager.pending_destroy_count() == 0);
    CHECK(index.empty());
    CHECK(delayed_destroy_actor::destroyed_count == 2);
  }

  const caors::memory_statistics statistics = tracking.statistics();
  CHECK(statistics.bytes_in_use == 0);
  CHECK(statistics.allocation_count == statistics.deallocation_count);
}

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
