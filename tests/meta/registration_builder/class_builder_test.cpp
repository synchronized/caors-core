// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：meta_builder_registration_accepts_fields_with_static_only_types_test.cpp

#include "meta_property_test_support.h"

using namespace test_meta;

TEST_CASE("meta - registration builder accepts fields with unregistered static types", "[meta]") {
  struct unregistered_field {};
  struct owner {
    unregistered_field field{};
  };

  REQUIRE(caors::registration::type_<owner>("caors.test.meta.owner")
              .meta()
              .property("field", &owner::field)
              .commit() == caors::error{});

  auto reflected = caors::meta::find("caors.test.meta.owner");
  REQUIRE(reflected);

  auto field = reflected.find_property("field");
  REQUIRE(field);
  CHECK(field.get_type().name().empty());
  CHECK_FALSE(caors::meta::find(field.get_type().id()).registered());
  CHECK(field.get_value_type().id() == field.get_type().id());
  CHECK_FALSE(field.indirect());

  owner instance{};
  CHECK(field.try_get<unregistered_field>(caors::meta::view(instance)).has_value());
}

// 原测试文件：meta_builder_registration_infers_built_in_field_types_test.cpp

#include "meta_property_test_support.h"

using namespace test_meta;

TEST_CASE("meta - registration builder infers built-in field types", "[meta]") {
  const caors::error result =
      caors::registration::type_<builder_actor>("caors.test.meta.builder_actor")
          .meta()
          .property("id", &builder_actor::id)
          .property("health", &builder_actor::health)
          .commit();

  REQUIRE(result == caors::error{});

  auto reflected = caors::meta::find("caors.test.meta.builder_actor");
  REQUIRE(reflected);

  auto id = reflected.find_property("id");
  REQUIRE(id);
  CHECK(id.get_type().name() == "int");
  CHECK(id.get_type().id() == registered_type_id("int"));

  auto health = reflected.find_property("health");
  REQUIRE(health);
  CHECK(health.get_type().name() == "float");
  CHECK(health.get_type().id() == registered_type_id("float"));

  builder_actor instance{3, 80.0F};
  CHECK(id.try_set<int>(caors::meta::view(instance), 10));
  CHECK(health.try_set<float>(caors::meta::view(instance), 20.0F));
  CHECK(instance.id == 10);
  CHECK(instance.health == 20.0F);
}

// 原测试文件：meta_builder_registration_resolves_explicitly_registered_c_field_type_test.cpp

#include "meta_property_test_support.h"

using namespace test_meta;

TEST_CASE("meta - registration builder resolves explicitly registered C++ field type", "[meta]") {
  require_registered_type<test_meta::named_field_value>("caors.test.meta.named_field_value");

  REQUIRE(
      caors::registration::type_<test_meta::named_field_owner>("caors.test.meta.named_field_owner")
          .meta()
          .property("value", &test_meta::named_field_owner::value)
          .commit() == caors::error{});

  auto value_type = caors::meta::find("caors.test.meta.named_field_value");
  REQUIRE(value_type);
  CHECK(value_type.id() == caors::meta::id_of<test_meta::named_field_value>());

  auto reflected = caors::meta::find("caors.test.meta.named_field_owner");
  REQUIRE(reflected);

  auto value = reflected.find_property("value");
  REQUIRE(value);
  CHECK(value.get_type().name() == "caors.test.meta.named_field_value");
  CHECK(value.get_type().id() == value_type.id());
}

// 原测试文件：meta_builder_registration_resolves_registered_c_field_types_test.cpp

#include "meta_property_test_support.h"

using namespace test_meta;

TEST_CASE("meta - registration builder resolves registered C++ field types", "[meta]") {
  REQUIRE(caors::registration::type_<inventory>("caors.test.meta.inventory")
              .meta()
              .property("count", &inventory::count)
              .commit() == caors::error{});

  REQUIRE(caors::registration::type_<inventory_holder>("caors.test.meta.inventory_holder")
              .meta()
              .property("items", &inventory_holder::items)
              .commit() == caors::error{});

  auto reflected = caors::meta::find("caors.test.meta.inventory_holder");
  REQUIRE(reflected);

  auto items = reflected.find_property("items");
  REQUIRE(items);
  CHECK(items.get_type().name() == "caors.test.meta.inventory");
  CHECK(items.get_type().id() == registered_type_id("caors.test.meta.inventory"));
}
