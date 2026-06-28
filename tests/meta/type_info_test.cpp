// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <catch2/catch_all.hpp>

#include <string_view>

namespace {

struct sample_type {
  int value = 0;
};

struct registry_parent_type {};

struct cpp_sample_type {};

struct guid_parent_type {};

struct error_parent_type {};

struct error_child_type : error_parent_type {};

struct guid_owner_type {};

struct duplicate_guid_type {};

struct relation_parent_type {};

struct relation_child_type : relation_parent_type {};

}

TEST_CASE("type info - built-in C++ types have default names", "[meta]") {
  CHECK(caors::type_name<int>::value == "int");
  CHECK(caors::type_name<float>::value == "float");
  CHECK(caors::type_name<bool>::value == "bool");
}

TEST_CASE("type info - type id is generated from names", "[meta]") {
  const caors::type_id first = caors::make_type_id("caors.test.sample");
  const caors::type_id second = caors::make_type_id("caors.test.sample");
  const caors::type_id other = caors::make_type_id("caors.test.other");

  CHECK(first);
  CHECK(first == second);
  CHECK(first != other);
}

TEST_CASE("type info - describes layout", "[meta]") {
  constexpr std::string_view name = "caors.test.sample";
  const caors::type_info info = caors::make_type_info<sample_type>(name);

  CHECK(info.valid());
  CHECK(info.id == caors::make_type_id(name));
  CHECK(info.name == name);
  CHECK_FALSE(info.guid);
  CHECK(info.size == sizeof(sample_type));
  CHECK(info.alignment == alignof(sample_type));
  CHECK_FALSE(info.parent_id);
}

TEST_CASE("type info - describes stable type guid", "[meta]") {
  const auto guid = caors::guid::parse("5ed39a97-7b20-4d87-86ac-e62a4a615b10");
  REQUIRE(guid.has_value());

  const caors::type_info info =
      caors::make_type_info<sample_type>("caors.test.guid.sample", caors::type_guid{*guid});

  CHECK(info.valid());
  CHECK(info.id == caors::make_type_id("caors.test.guid.sample"));
  CHECK(info.guid == caors::type_guid{*guid});
  CHECK(info.guid.value.string() == "5ed39a97-7b20-4d87-86ac-e62a4a615b10");
}

TEST_CASE("type info - global registry registers and finds types", "[meta]") {
  CHECK(caors::register_type<registry_parent_type>("caors.test.type.registry_parent") ==
        caors::type_register_result::success);
  CHECK(caors::register_type<registry_parent_type>("caors.test.type.registry_parent") ==
        caors::type_register_result::duplicate_id);

  auto type = caors::find_type("caors.test.type.registry_parent");
  REQUIRE(type.has_value());
  CHECK(type->id);
  CHECK(type->name == "caors.test.type.registry_parent");
  CHECK(type->size == sizeof(registry_parent_type));
  CHECK(type->alignment == alignof(registry_parent_type));

  auto by_name = caors::find_type("caors.test.type.registry_parent");
  REQUIRE(by_name.has_value());
  CHECK(by_name->id == type->id);
}

TEST_CASE("type info - global registry finds types by C++ type", "[meta]") {
  REQUIRE(caors::register_type<cpp_sample_type>("caors.test.type.cpp_sample") ==
          caors::type_register_result::success);

  auto type = caors::find_type<cpp_sample_type>();
  REQUIRE(type.has_value());
  CHECK(type->name == "caors.test.type.cpp_sample");
  CHECK(type->id == caors::make_type_id("caors.test.type.cpp_sample"));
}

TEST_CASE("type info - global registry finds types by stable guid", "[meta]") {
  const auto guid = caors::guid::parse("7b458821-d56a-4133-a5a6-90a8661ee474");
  REQUIRE(guid.has_value());

  REQUIRE(caors::register_type<guid_parent_type>("caors.test.type.guid_parent",
                                                 caors::type_guid{*guid}) ==
          caors::type_register_result::success);

  auto type = caors::find_type(caors::type_guid{*guid});
  REQUIRE(type.has_value());
  CHECK(type->name == "caors.test.type.guid_parent");
  CHECK(type->guid == caors::type_guid{*guid});
}

TEST_CASE("type info - global registry reports registration errors", "[meta]") {
  CHECK(caors::get_meta_registry().register_type(caors::type_info{}) ==
        caors::type_register_result::invalid_type);

  caors::type_info missing_parent = caors::make_type_info<error_child_type>(
      "caors.test.type.missing_parent_child",
      caors::make_type_id("caors.test.type.missing_parent"));
  CHECK(caors::get_meta_registry().register_type(missing_parent) ==
        caors::type_register_result::missing_parent);

  REQUIRE(caors::register_type<error_parent_type>("caors.test.type.error_parent") ==
          caors::type_register_result::success);
  const auto parent = caors::find_type<error_parent_type>();
  REQUIRE(parent.has_value());

  caors::type_info colliding =
      caors::make_type_info<error_child_type>("caors.test.type.error_child");
  colliding.id = parent->id;
  CHECK(caors::get_meta_registry().register_type(colliding) ==
        caors::type_register_result::invalid_type);

  caors::type_info duplicate_name =
      caors::make_type_info<sample_type>("caors.test.type.unique_duplicate_name");
  duplicate_name.name = caors::string_name{"caors.test.type.error_parent"};
  duplicate_name.id = caors::make_type_id(duplicate_name.name);
  CHECK(caors::get_meta_registry().register_type(duplicate_name) ==
        caors::type_register_result::duplicate_name);

  const auto guid = caors::guid::parse("c9e7a3da-2cc0-4048-97ef-d9327f71b09e");
  REQUIRE(guid.has_value());
  REQUIRE(caors::register_type<guid_owner_type>("caors.test.type.guid_owner",
                                                caors::type_guid{*guid}) ==
          caors::type_register_result::success);

  caors::type_info duplicate_guid = caors::make_type_info<duplicate_guid_type>(
      "caors.test.type.duplicate_guid", caors::type_guid{*guid});
  CHECK(caors::get_meta_registry().register_type(duplicate_guid) ==
        caors::type_register_result::duplicate_guid);
}

TEST_CASE("type info - global registry checks parent relationship", "[meta]") {
  REQUIRE(caors::register_type<relation_parent_type>("caors.test.type.relation_parent") ==
          caors::type_register_result::success);
  const auto parent = caors::find_type("caors.test.type.relation_parent");
  REQUIRE(parent.has_value());

  REQUIRE(caors::register_type<relation_child_type>("caors.test.type.relation_child", parent->id) ==
          caors::type_register_result::success);
  const auto child = caors::find_type("caors.test.type.relation_child");
  REQUIRE(child.has_value());

  CHECK(caors::get_meta_registry().contains(parent->id));
  CHECK(caors::get_meta_registry().contains(child->id));
  CHECK(caors::is_a(child->id, child->id));
  CHECK(caors::is_a(child->id, parent->id));
  CHECK_FALSE(caors::is_a(parent->id, child->id));
}
