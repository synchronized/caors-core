// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：meta_registry_class_builder_can_attach_to_explicit_meta_type_test.cpp

#include "meta_registry_view_test_support.h"

using namespace test_meta;

TEST_CASE("meta - registration builder commits explicit meta type", "[meta]") {
  const caors::error status =
      caors::registration::type_<explicit_meta_actor>("caors.test.meta.explicit_meta_actor")
          .meta()
          .property("id", &explicit_meta_actor::id)
          .commit();
  REQUIRE(status == caors::error{});

  const caors::meta::type type = caors::meta::find<explicit_meta_actor>();
  REQUIRE(type.valid());

  auto reflected = caors::meta::find(type.id());
  REQUIRE(reflected);
  CHECK(reflected.property_count() == 1);
  CHECK(reflected.find_property("id"));
}

TEST_CASE("meta - registration builder requires a name", "[meta]") {
  const caors::error status = caors::registration::type_<invalid_field_owner>("")
                                  .meta()
                                  .property("id", &invalid_field_owner::id)
                                  .commit();

  CHECK(status.failed());
  CHECK(status.domain() == caors::error_domain::meta);
  CHECK(status.value() == caors::errc::invalid_argument);
}

// 原测试文件：meta_registry_registry_registers_and_finds_types_test.cpp

#include "meta_registry_view_test_support.h"

using namespace test_meta;

TEST_CASE("meta - registry registers and finds types", "[meta]") {
  REQUIRE(register_registry_actor() == caors::error{});

  CHECK(caors::meta::get_registry().size() >= 1);

  auto by_name = caors::meta::find("caors.test.meta.registry_actor");
  REQUIRE(by_name);
  CHECK(caors::meta::get_registry().contains(by_name.id()));
  CHECK(by_name.property_count() == 2);

  auto by_guid = caors::meta::find(registry_actor_guid);
  REQUIRE(by_guid);
  CHECK(by_guid.id() == by_name.id());
}

// 原测试文件：meta_registry_registry_registers_child_type_with_parent_test.cpp

#include "meta_registry_view_test_support.h"

using namespace test_meta;

TEST_CASE("meta - registry registers child type with parent", "[meta]") {
  REQUIRE(caors::registration::type_<parent_actor>("caors.test.meta.parent_actor")
              .meta()
              .property("id", &parent_actor::id)
              .commit() == caors::error{});
  const auto reflected_actor = caors::meta::find("caors.test.meta.parent_actor");
  REQUIRE(reflected_actor);

  REQUIRE(caors::registration::type_<child_player>("caors.test.meta.child_player")
              .parent(reflected_actor.id())
              .meta()
              .property("level", &child_player::level)
              .commit() == caors::error{});

  auto reflected_player = caors::meta::find("caors.test.meta.child_player");
  REQUIRE(reflected_player);
  CHECK(reflected_player.parent_id() == reflected_actor.id());
}

// 原测试文件：meta_registry_registry_reports_duplicate_field_test.cpp

#include "meta_registry_view_test_support.h"

using namespace test_meta;

TEST_CASE("meta - registry reports duplicate field", "[meta]") {
  REQUIRE(caors::registration::type_<duplicate_actor>("caors.test.meta.duplicate_actor")
              .meta()
              .property("id", &duplicate_actor::id)
              .commit() == caors::error{});
  CHECK(caors::registration::type_<duplicate_actor>("caors.test.meta.duplicate_actor")
            .meta()
            .property("id", &duplicate_actor::id)
            .commit() == meta_error(caors::errc::duplicate_id));
}

// 原测试文件：meta_registry_registry_returns_type_and_field_views_test.cpp

#include "meta_registry_view_test_support.h"

using namespace test_meta;

TEST_CASE("meta - registry returns type and property views", "[meta]") {
  const caors::error register_status = register_view_actor();
  REQUIRE((register_status == caors::error{} ||
           register_status == meta_error(caors::errc::duplicate_id)));

  auto reflected = caors::meta::find("caors.test.meta.view_actor");
  REQUIRE(reflected);
  CHECK(reflected.valid());
  CHECK(reflected.id() == registered_type_id("caors.test.meta.view_actor"));
  CHECK(reflected.name() == "caors.test.meta.view_actor");
  CHECK(reflected.guid() == view_actor_guid);
  CHECK(reflected.property_count() == 2);

  auto id = reflected.property_at(0);
  REQUIRE(id);
  CHECK(id.valid());
  CHECK(id.name() == "id");
  CHECK(id.get_type().name() == "int");
  CHECK(id.get_value_type().id() == registered_type_id("int"));
  CHECK_FALSE(reflected.property_at(99));

  auto health = reflected.find_property("health");
  REQUIRE(health);
  CHECK(health.name() == "health");
  CHECK(health.kind() == caors::meta::type_kind::value);
  CHECK(health.wrapper_semantics() == caors::meta::wrapper_semantics::none);
  CHECK_FALSE(health.indirect());
  CHECK_FALSE(health.nullable());
  CHECK(health.writable());
  CHECK(health.offset() == offsetof(view_actor, health));
  CHECK(health.size() == sizeof(float));
  CHECK(health.alignment() == alignof(float));

  view_actor instance{1, 25.0F};
  CHECK(health.try_set<float>(caors::meta::view(instance), 90.0F));
  CHECK(instance.health == 90.0F);

  caors::any health_value = health.get_value(caors::meta::view(instance));
  REQUIRE(health_value.valid());
  const float* copied_health = health_value.try_get<float>();
  REQUIRE(copied_health != nullptr);
  CHECK(*copied_health == 90.0F);

  const int next_id = 7;
  CHECK(id.try_set_value(caors::meta::view(instance), caors::any::from(next_id)));
  CHECK(instance.id == 7);
}

// 原测试文件：meta_registry_registry_validates_duplicate_fields_test.cpp

#include "meta_registry_view_test_support.h"

using namespace test_meta;

TEST_CASE("meta - registry validates duplicate fields", "[meta]") {
  CHECK(caors::registration::type_<duplicate_field_owner>("caors.test.meta.duplicate_field")
            .meta()
            .property("id", &duplicate_field_owner::id)
            .property("id", &duplicate_field_owner::health)
            .commit() == meta_error(caors::errc::duplicate_field));
}

// 原测试文件：meta_registry_views_remain_valid_for_append_only_registry_test.cpp

#include "meta_registry_view_test_support.h"

using namespace test_meta;

TEST_CASE("meta - views remain valid for append-only registry", "[meta]") {
  const caors::error register_status = register_view_actor();
  REQUIRE((register_status == caors::error{} ||
           register_status == meta_error(caors::errc::duplicate_id)));

  auto reflected = caors::meta::find("caors.test.meta.view_actor");
  REQUIRE(reflected);

  auto field = reflected.find_property("id");
  REQUIRE(field);
  CHECK(reflected.valid());
  CHECK(field.valid());

  CHECK(register_view_actor() == meta_error(caors::errc::duplicate_id));

  CHECK(reflected.valid());
  CHECK(field.valid());
}
