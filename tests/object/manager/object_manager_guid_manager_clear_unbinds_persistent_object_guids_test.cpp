// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

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
