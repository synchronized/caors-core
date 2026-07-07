// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_registry_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - registry registers child type with parent", "[reflection]") {
  REQUIRE(caors::reflection::class_<parent_actor>("caors.test.reflection.parent_actor")
              .field("id", &parent_actor::id)
              .commit() == caors::error{});
  const auto reflected_actor = caors::reflection::find("caors.test.reflection.parent_actor");
  REQUIRE(reflected_actor);

  REQUIRE(caors::reflection::class_<child_player>("caors.test.reflection.child_player",
                                                  reflected_actor.id())
              .field("level", &child_player::level)
              .commit() == caors::error{});

  auto reflected_player = caors::reflection::find("caors.test.reflection.child_player");
  REQUIRE(reflected_player);
  CHECK(reflected_player.info().parent_id() == reflected_actor.id());
}
