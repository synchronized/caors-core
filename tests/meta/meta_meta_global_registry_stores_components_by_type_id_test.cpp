// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_test_support.h"

using namespace caors::tests::meta_registry;
TEST_CASE("meta - global registry stores components by type id", "[meta]") {
  REQUIRE(caors::meta::type_<component_actor_type>("caors.test.meta.component_actor").result() ==
          caors::error{});
  const auto actor = caors::meta::find<component_actor_type>();
  REQUIRE(actor);
  const caors::type_id actor_id = actor.id();

  CHECK(caors::meta::get_registry().emplace<factory_meta>(actor_id, true) == caors::error{});
  CHECK(caors::meta::get_registry().has<factory_meta>(actor_id));
  CHECK(caors::meta::get_registry().count<factory_meta>() == 1);

  auto factory = caors::meta::get_registry().find<factory_meta>(actor_id);
  REQUIRE(factory.has_value());
  CHECK(factory->constructible);

  const factory_meta* factory_ptr = caors::meta::get_registry().find_ptr<factory_meta>(actor_id);
  REQUIRE(factory_ptr != nullptr);
  CHECK(factory_ptr->constructible);

  CHECK(caors::meta::get_registry().emplace<factory_meta>(actor_id, false) ==
        meta_error(caors::errc::duplicate_component));
  CHECK(caors::meta::get_registry().has<factory_meta>(actor_id));
  CHECK(caors::meta::get_registry().component_status<factory_meta>(actor_id) ==
        caors::meta::component_status::present);
  CHECK(caors::meta::get_registry().component_status<view_reflection_meta>(actor_id) ==
        caors::meta::component_status::missing_component);
  CHECK(caors::meta::get_registry().component_status<factory_meta>(caors::type_id::invalid()) ==
        caors::meta::component_status::invalid_type);
  CHECK(caors::meta::get_registry().component_status<factory_meta>(
            caors::type_id{std::numeric_limits<std::uint64_t>::max()}) ==
        caors::meta::component_status::missing_type);
}
