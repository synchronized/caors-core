// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

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
