// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <limits>
#include <string_view>

namespace {

struct sample_type {
  int value = 0;
};

struct anonymous_cpp_type {};

struct registry_parent_type {};

struct cpp_sample_type {};

struct guid_parent_type {};

struct error_parent_type {};

struct error_child_type : error_parent_type {};

struct guid_owner_type {};

struct duplicate_guid_type {};

struct relation_parent_type {};

struct relation_child_type : relation_parent_type {};

struct lazy_meta_builder_type {};

struct fluent_meta_type {};

struct explicit_get_type {};

struct missing_get_type {};

[[nodiscard]] constexpr caors::error meta_error(caors::errc code) noexcept {
  return {caors::error_domain::meta, code};
}

}

TEST_CASE("meta type - built-in C++ types have default names", "[meta]") {
  CHECK(caors::type_name<int>::value == "int");
  CHECK(caors::type_name<float>::value == "float");
  CHECK(caors::type_name<bool>::value == "bool");
}

TEST_CASE("meta type - type id is allocated by explicit registration", "[meta]") {
  CHECK_FALSE(caors::meta::id("caors.test.type.unregistered_name"));
  CHECK_FALSE(caors::meta::id_of<anonymous_cpp_type>());
  CHECK_FALSE(caors::meta::find<anonymous_cpp_type>().has_value());

  REQUIRE(caors::meta::detail::register_type<anonymous_cpp_type>("caors.test.type.anonymous_cpp") ==
          caors::error{});

  const caors::type_id first = caors::meta::id_of<anonymous_cpp_type>();
  const caors::type_id second = caors::meta::id_of<anonymous_cpp_type>();

  CHECK(first);
  CHECK(first == second);

  auto named = caors::meta::find("caors.test.type.anonymous_cpp");
  REQUIRE(named.has_value());
  CHECK(named->id() == first);
  CHECK(named->cpp_type() == caors::detail::cpp_type_key_of<anonymous_cpp_type>());
  CHECK(named->clone_value() != nullptr);
  CHECK(named->destroy_value() != nullptr);
  CHECK(named->assign_value() != nullptr);
  CHECK(caors::meta::id("caors.test.type.anonymous_cpp") == first);
}

TEST_CASE("meta type data - describes layout", "[meta]") {
  constexpr std::string_view name = "caors.test.sample";
  const caors::meta::type_data data = caors::meta::make_type_data<sample_type>(name);

  CHECK(data.valid());
  CHECK_FALSE(data.registered());
  CHECK_FALSE(data.id());
  CHECK(data.name() == name);
  CHECK_FALSE(data.guid());
  CHECK(data.size() == sizeof(sample_type));
  CHECK(data.alignment() == alignof(sample_type));
  CHECK_FALSE(data.parent_id());
  CHECK(data.cpp_type() == caors::detail::cpp_type_key_of<sample_type>());
  CHECK(data.default_construct_value() != nullptr);
  CHECK(data.clone_value() != nullptr);
  CHECK(data.destroy_value() != nullptr);
  CHECK(data.assign_value() != nullptr);
}

TEST_CASE("meta type data - describes stable type guid", "[meta]") {
  const auto guid = caors::guid::parse("5ed39a97-7b20-4d87-86ac-e62a4a615b10");
  REQUIRE(guid.has_value());

  const caors::meta::type_data data =
      caors::meta::make_type_data<sample_type>("caors.test.guid.sample", caors::type_guid{*guid});

  CHECK(data.valid());
  CHECK_FALSE(data.registered());
  CHECK_FALSE(data.id());
  CHECK(data.guid() == caors::type_guid{*guid});
  CHECK(data.guid().value.string() == "5ed39a97-7b20-4d87-86ac-e62a4a615b10");
}

TEST_CASE("meta type - global registry registers and finds types", "[meta]") {
  CHECK(caors::meta::detail::register_type<registry_parent_type>(
            "caors.test.type.registry_parent") == caors::error{});
  CHECK(caors::meta::detail::register_type<registry_parent_type>(
            "caors.test.type.registry_parent") == meta_error(caors::errc::duplicate_id));

  auto type = caors::meta::find("caors.test.type.registry_parent");
  REQUIRE(type.has_value());
  CHECK(type->id());
  CHECK(type->name() == "caors.test.type.registry_parent");
  CHECK(type->size() == sizeof(registry_parent_type));
  CHECK(type->alignment() == alignof(registry_parent_type));

  auto by_name = caors::meta::find("caors.test.type.registry_parent");
  REQUIRE(by_name.has_value());
  CHECK(by_name->id() == type->id());
}

TEST_CASE("meta type - global registry finds types by C++ type", "[meta]") {
  REQUIRE(caors::meta::detail::register_type<cpp_sample_type>("caors.test.type.cpp_sample") ==
          caors::error{});

  auto type = caors::meta::find<cpp_sample_type>();
  REQUIRE(type.has_value());
  CHECK(type->name() == "caors.test.type.cpp_sample");
  CHECK(caors::meta::id("caors.test.type.cpp_sample") == type->id());
}

TEST_CASE("meta type - global registry finds types by stable guid", "[meta]") {
  const auto guid = caors::guid::parse("7b458821-d56a-4133-a5a6-90a8661ee474");
  REQUIRE(guid.has_value());

  REQUIRE(caors::meta::detail::register_type<guid_parent_type>(
              "caors.test.type.guid_parent", caors::type_guid{*guid}) == caors::error{});

  auto type = caors::meta::find(caors::type_guid{*guid});
  REQUIRE(type.has_value());
  CHECK(type->name() == "caors.test.type.guid_parent");
  CHECK(type->guid() == caors::type_guid{*guid});
}

TEST_CASE("meta type - fluent builder registers name and guid", "[meta]") {
  const auto guid = caors::guid::parse("d2953377-4ffc-4c81-aeef-7e020272a536");
  REQUIRE(guid.has_value());

  auto builder = caors::meta::type<fluent_meta_type>()
                     .type("caors.test.type.fluent_meta")
                     .guid(caors::type_guid{*guid});

  CHECK(builder.succeeded());
  const caors::meta::type_view type = builder.get();
  REQUIRE(type);
  CHECK(type.name() == "caors.test.type.fluent_meta");
  CHECK(type.guid() == caors::type_guid{*guid});
  CHECK(caors::meta::find<fluent_meta_type>()->id() == type.id());
  CHECK(caors::meta::find(caors::type_guid{*guid})->id() == type.id());
}

TEST_CASE("meta type - builder without name does not register type", "[meta]") {
  const auto builder = caors::meta::type<lazy_meta_builder_type>();

  CHECK_FALSE(builder.get());
  CHECK_FALSE(caors::meta::get<lazy_meta_builder_type>());
  CHECK_FALSE(caors::meta::find<lazy_meta_builder_type>().has_value());
}

TEST_CASE("meta type - explicit type access does not create missing types", "[meta]") {
  CHECK_FALSE(caors::meta::get<missing_get_type>());

  const caors::meta::type_view type =
      caors::meta::type<explicit_get_type>("caors.test.type.explicit_get").get();
  REQUIRE(type.valid());

  const caors::meta::type_view found = caors::meta::get<explicit_get_type>();
  REQUIRE(found.valid());
  CHECK(found.id() == type.id());
}

TEST_CASE("meta type - global registry reports registration errors", "[meta]") {
  CHECK(caors::meta::get_registry().register_type(caors::meta::type_data{}) ==
        meta_error(caors::errc::invalid_type));

  caors::meta::type_data missing_parent = caors::meta::make_type_data<error_child_type>(
      "caors.test.type.missing_parent_child",
      caors::type_id{std::numeric_limits<std::uint64_t>::max()});
  CHECK(caors::meta::get_registry().register_type(missing_parent) ==
        meta_error(caors::errc::missing_parent));

  REQUIRE(caors::meta::detail::register_type<error_parent_type>("caors.test.type.error_parent") ==
          caors::error{});
  const auto parent = caors::meta::find<error_parent_type>();
  REQUIRE(parent.has_value());

  caors::meta::type_data colliding =
      caors::meta::make_type_data<error_child_type>("caors.test.type.error_child");
  colliding.set_id(parent->id());
  CHECK(caors::meta::get_registry().register_type(colliding) ==
        meta_error(caors::errc::invalid_type));

  caors::meta::type_data duplicate_name =
      caors::meta::make_type_data<sample_type>("caors.test.type.unique_duplicate_name");
  duplicate_name.set_name(caors::string_name{"caors.test.type.error_parent"});
  CHECK(caors::meta::get_registry().register_type(duplicate_name) ==
        meta_error(caors::errc::duplicate_name));

  const auto guid = caors::guid::parse("c9e7a3da-2cc0-4048-97ef-d9327f71b09e");
  REQUIRE(guid.has_value());
  REQUIRE(caors::meta::detail::register_type<guid_owner_type>(
              "caors.test.type.guid_owner", caors::type_guid{*guid}) == caors::error{});

  caors::meta::type_data duplicate_guid = caors::meta::make_type_data<duplicate_guid_type>(
      "caors.test.type.duplicate_guid", caors::type_guid{*guid});
  CHECK(caors::meta::get_registry().register_type(duplicate_guid) ==
        meta_error(caors::errc::duplicate_guid));
}

TEST_CASE("meta type - global registry checks parent relationship", "[meta]") {
  REQUIRE(caors::meta::detail::register_type<relation_parent_type>(
              "caors.test.type.relation_parent") == caors::error{});
  const auto parent = caors::meta::find("caors.test.type.relation_parent");
  REQUIRE(parent.has_value());

  REQUIRE(caors::meta::detail::register_type<relation_child_type>("caors.test.type.relation_child",
                                                                  parent->id()) == caors::error{});
  const auto child = caors::meta::find("caors.test.type.relation_child");
  REQUIRE(child.has_value());

  CHECK(caors::meta::get_registry().contains(parent->id()));
  CHECK(caors::meta::get_registry().contains(child->id()));
  CHECK(caors::meta::is_a(child->id(), child->id()));
  CHECK(caors::meta::is_a(child->id(), parent->id()));
  CHECK_FALSE(caors::meta::is_a(parent->id(), child->id()));
  CHECK(child->parent().id() == parent->id());
}
