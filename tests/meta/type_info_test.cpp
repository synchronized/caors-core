// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <catch2/catch_all.hpp>

#include <string_view>

namespace {

struct sample_type {
  int value = 0;
};

struct parent_type {};

struct child_type : parent_type {};

}

TEST_CASE("type info - type id is generated from stable names", "[meta]") {
  const caors::type_id first = caors::make_type_id("caors.test.sample");
  const caors::type_id second = caors::make_type_id("caors.test.sample");
  const caors::type_id other = caors::make_type_id("caors.test.other");

  CHECK(first);
  CHECK(first == second);
  CHECK(first != other);
}

TEST_CASE("type info - describes layout and parent type", "[meta]") {
  constexpr std::string_view name = "caors.test.sample";
  const caors::type_id parent_id = caors::make_type_id("caors.test.parent");
  const caors::type_info info = caors::make_type_info<sample_type>(name, parent_id);

  CHECK(info.valid());
  CHECK(info.id == caors::make_type_id(name));
  CHECK(info.name == name);
  CHECK_FALSE(info.guid);
  CHECK(info.size == sizeof(sample_type));
  CHECK(info.alignment == alignof(sample_type));
  CHECK(info.parent_id == parent_id);
}

TEST_CASE("type info - describes stable type guid", "[meta]") {
  const auto guid = caors::guid::parse("5ed39a97-7b20-4d87-86ac-e62a4a615b10");
  REQUIRE(guid.has_value());

  const caors::type_info info =
      caors::make_type_info<sample_type>("caors.test.guid.sample", caors::type_guid{*guid});

  CHECK(info.valid());
  CHECK(info.guid == caors::type_guid{*guid});
  CHECK(info.guid.value.string() == "5ed39a97-7b20-4d87-86ac-e62a4a615b10");
}

TEST_CASE("type info - registry registers and finds types", "[meta]") {
  caors::meta_registry registry;

  CHECK(caors::register_type<parent_type>(registry, "caors.test.type.parent") ==
        caors::type_register_result::success);
  CHECK(caors::register_type<parent_type>(registry, "caors.test.type.parent") ==
        caors::type_register_result::duplicate_id);

  auto type = registry.find_type(caors::make_type_id("caors.test.type.parent"));
  REQUIRE(type.has_value());
  CHECK(type->id == caors::make_type_id("caors.test.type.parent"));
  CHECK(type->name == "caors.test.type.parent");
  CHECK(type->size == sizeof(parent_type));
  CHECK(type->alignment == alignof(parent_type));

  auto by_name = registry.find_type("caors.test.type.parent");
  REQUIRE(by_name.has_value());
  CHECK(by_name->id == type->id);
}

TEST_CASE("type info - registry finds types by stable guid", "[meta]") {
  caors::meta_registry registry;
  const auto guid = caors::guid::parse("7b458821-d56a-4133-a5a6-90a8661ee474");
  REQUIRE(guid.has_value());

  REQUIRE(caors::register_type<parent_type>(registry, "caors.test.type.guid_parent",
                                            caors::type_guid{*guid}) ==
          caors::type_register_result::success);

  auto type = registry.find_type(caors::type_guid{*guid});
  REQUIRE(type.has_value());
  CHECK(type->name == "caors.test.type.guid_parent");
  CHECK(type->guid == caors::type_guid{*guid});
}

TEST_CASE("type info - registry reports registration errors", "[meta]") {
  caors::meta_registry registry;

  CHECK(registry.register_type(caors::type_info{}) == caors::type_register_result::invalid_type);

  caors::type_info missing_parent =
      caors::make_type_info<child_type>("caors.test.type.missing_parent_child",
                                        caors::make_type_id("caors.test.type.missing_parent"));
  CHECK(registry.register_type(missing_parent) == caors::type_register_result::missing_parent);

  REQUIRE(caors::register_type<parent_type>(registry, "caors.test.type.parent") ==
          caors::type_register_result::success);

  caors::type_info colliding = caors::make_type_info<child_type>("caors.test.type.child");
  colliding.id = caors::make_type_id("caors.test.type.parent");
  CHECK(registry.register_type(colliding) == caors::type_register_result::hash_collision);

  caors::type_info duplicate_name =
      caors::make_type_info<child_type>("caors.test.type.unique_duplicate_name");
  duplicate_name.name = caors::string_name{"caors.test.type.parent"};
  CHECK(registry.register_type(duplicate_name) == caors::type_register_result::duplicate_name);

  const auto guid = caors::guid::parse("c9e7a3da-2cc0-4048-97ef-d9327f71b09e");
  REQUIRE(guid.has_value());
  REQUIRE(caors::register_type<sample_type>(registry, "caors.test.type.guid_owner",
                                            caors::type_guid{*guid}) ==
          caors::type_register_result::success);

  caors::type_info duplicate_guid =
      caors::make_type_info<child_type>("caors.test.type.duplicate_guid", caors::type_guid{*guid});
  CHECK(registry.register_type(duplicate_guid) == caors::type_register_result::duplicate_guid);
}

TEST_CASE("type info - registry checks parent relationship", "[meta]") {
  caors::meta_registry registry;
  const caors::type_id parent_id = caors::make_type_id("caors.test.type.parent");
  const caors::type_id child_id = caors::make_type_id("caors.test.type.child");

  REQUIRE(caors::register_type<parent_type>(registry, "caors.test.type.parent") ==
          caors::type_register_result::success);
  REQUIRE(caors::register_type<child_type>(registry, "caors.test.type.child", parent_id) ==
          caors::type_register_result::success);

  CHECK(registry.contains(parent_id));
  CHECK(registry.contains(child_id));
  CHECK(registry.is_a(child_id, child_id));
  CHECK(registry.is_a(child_id, parent_id));
  CHECK_FALSE(registry.is_a(parent_id, child_id));
}
