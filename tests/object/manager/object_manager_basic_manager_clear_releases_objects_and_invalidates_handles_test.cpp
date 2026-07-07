// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

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
