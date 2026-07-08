// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - typed base exposes runtime type information", "[object]") {
  const caors::error actor_status = register_object_test_type<runtime_actor>();
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));
  REQUIRE(register_object_test_type<runtime_player>() == caors::error{});

  runtime_player instance;
  caors::object& base = instance;

  CHECK(runtime_player::static_type_id());
  CHECK(runtime_player::static_type_id() == caors::meta::id_of<runtime_player>());
  CHECK(base.type_id() == runtime_player::static_type_id());
  CHECK(base.type_name() == runtime_player::object_type_name);
}
