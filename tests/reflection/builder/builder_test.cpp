// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：reflection_builder_class_builder_accepts_fields_with_descriptor_only_types_test.cpp

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - class builder accepts fields with descriptor-only types", "[reflection]") {
  struct unregistered_field {};
  struct owner {
    unregistered_field field{};
  };

  REQUIRE(((void)caors::meta::type_<owner>("caors.test.reflection.owner"),
           caors::reflection::class_<owner>())
              .field("field", &owner::field)
              .commit() == caors::error{});

  auto reflected = caors::reflection::find("caors.test.reflection.owner");
  REQUIRE(reflected);

  auto field = reflected.find_field("field");
  REQUIRE(field);
  CHECK(field.get_type().name().empty());
  CHECK_FALSE(caors::meta::find(field.get_type().id()).registered());
  CHECK(field.get_value_type().id() == field.get_type().id());
  CHECK_FALSE(field.indirect());

  owner instance{};
  CHECK(field.try_get<unregistered_field>(&instance).has_value());
}

// 原测试文件：reflection_builder_class_builder_infers_built_in_field_types_test.cpp

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - class builder infers built-in field types", "[reflection]") {
  const caors::error result =
      ((void)caors::meta::type_<builder_actor>("caors.test.reflection.builder_actor"),
       caors::reflection::class_<builder_actor>())
          .field("id", &builder_actor::id)
          .field("health", &builder_actor::health)
          .commit();

  REQUIRE(result == caors::error{});

  auto reflected = caors::reflection::find("caors.test.reflection.builder_actor");
  REQUIRE(reflected);

  auto id = reflected.find_field("id");
  REQUIRE(id);
  CHECK(id.get_type().name() == "int");
  CHECK(id.get_type().id() == registered_type_id("int"));

  auto health = reflected.find_field("health");
  REQUIRE(health);
  CHECK(health.get_type().name() == "float");
  CHECK(health.get_type().id() == registered_type_id("float"));

  builder_actor instance{3, 80.0F};
  CHECK(id.try_set<int>(&instance, 10));
  CHECK(health.try_set<float>(&instance, 20.0F));
  CHECK(instance.id == 10);
  CHECK(instance.health == 20.0F);
}

// 原测试文件：reflection_builder_class_builder_resolves_explicitly_registered_c_field_type_test.cpp

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - class builder resolves explicitly registered C++ field type",
          "[reflection]") {
  require_registered_type<test_reflection::named_field_value>(
      "caors.test.reflection.named_field_value");

  REQUIRE(((void)caors::meta::type_<test_reflection::named_field_owner>(
               "caors.test.reflection.named_field_owner"),
           caors::reflection::class_<test_reflection::named_field_owner>())
              .field("value", &test_reflection::named_field_owner::value)
              .commit() == caors::error{});

  auto value_type = caors::meta::find("caors.test.reflection.named_field_value");
  REQUIRE(value_type);
  CHECK(value_type.id() == caors::meta::id_of<test_reflection::named_field_value>());

  auto reflected = caors::reflection::find("caors.test.reflection.named_field_owner");
  REQUIRE(reflected);

  auto value = reflected.find_field("value");
  REQUIRE(value);
  CHECK(value.get_type().name() == "caors.test.reflection.named_field_value");
  CHECK(value.get_type().id() == value_type.id());
}

// 原测试文件：reflection_builder_class_builder_resolves_registered_c_field_types_test.cpp

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - class builder resolves registered C++ field types", "[reflection]") {
  REQUIRE(((void)caors::meta::type_<inventory>("caors.test.reflection.inventory"),
           caors::reflection::class_<inventory>())
              .field("count", &inventory::count)
              .commit() == caors::error{});

  REQUIRE(((void)caors::meta::type_<inventory_holder>("caors.test.reflection.inventory_holder"),
           caors::reflection::class_<inventory_holder>())
              .field("items", &inventory_holder::items)
              .commit() == caors::error{});

  auto reflected = caors::reflection::find("caors.test.reflection.inventory_holder");
  REQUIRE(reflected);

  auto items = reflected.find_field("items");
  REQUIRE(items);
  CHECK(items.get_type().name() == "caors.test.reflection.inventory");
  CHECK(items.get_type().id() == registered_type_id("caors.test.reflection.inventory"));
}
