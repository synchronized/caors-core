// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <limits>

namespace {

struct registry_parent_type {};

struct cpp_sample_type {};

struct guid_parent_type {};

struct error_parent_type {};

struct error_child_type : error_parent_type {};

struct duplicate_guid_type {};

struct duplicate_guid_owner_type {};

struct relation_parent_type {};

struct relation_child_type : relation_parent_type {};

[[nodiscard]] constexpr caors::error meta_error(caors::errc code) noexcept {
  return {caors::error_domain::meta, code};
}

}

TEST_CASE("meta type - global registry registers and finds types", "[meta]") {
  CHECK(caors::meta::type_<registry_parent_type>("caors.test.type.registry_parent").result() ==
        caors::error{});
  CHECK(caors::meta::type_<registry_parent_type>("caors.test.type.registry_parent").result() ==
        meta_error(caors::errc::duplicate_id));

  auto type = caors::meta::find("caors.test.type.registry_parent");
  REQUIRE(type);
  CHECK(type.id());
  CHECK(type.name() == "caors.test.type.registry_parent");
  CHECK(type.size() == sizeof(registry_parent_type));
  CHECK(type.alignment() == alignof(registry_parent_type));

  auto by_name = caors::meta::find("caors.test.type.registry_parent");
  REQUIRE(by_name);
  CHECK(by_name.id() == type.id());
}

TEST_CASE("meta type - global registry finds types by C++ type", "[meta]") {
  REQUIRE(caors::meta::type_<cpp_sample_type>("caors.test.type.cpp_sample").result() ==
          caors::error{});

  auto type = caors::meta::find<cpp_sample_type>();
  REQUIRE(type);
  CHECK(type.name() == "caors.test.type.cpp_sample");
  CHECK(caors::meta::id("caors.test.type.cpp_sample") == type.id());
}

TEST_CASE("meta type - global registry finds types by stable guid", "[meta]") {
  const auto guid = caors::guid::parse("7b458821-d56a-4133-a5a6-90a8661ee474");
  REQUIRE(guid.has_value());

  REQUIRE(caors::meta::type_<guid_parent_type>("caors.test.type.guid_parent")
              .guid(caors::type_guid{*guid})
              .result() == caors::error{});

  auto type = caors::meta::find(caors::type_guid{*guid});
  REQUIRE(type);
  CHECK(type.name() == "caors.test.type.guid_parent");
  CHECK(type.guid() == caors::type_guid{*guid});
}

TEST_CASE("meta type - global registry reports registration errors", "[meta]") {
  CHECK(caors::meta::type_<error_child_type>("caors.test.type.missing_parent_child")
            .parent(caors::type_id{std::numeric_limits<std::uint64_t>::max()})
            .result() == meta_error(caors::errc::missing_parent));

  REQUIRE(caors::meta::type_<error_parent_type>("caors.test.type.error_parent").result() ==
          caors::error{});

  CHECK(caors::meta::type_<error_child_type>("caors.test.type.error_parent").result() ==
        meta_error(caors::errc::duplicate_name));

  const auto guid = caors::guid::parse("c9e7a3da-2cc0-4048-97ef-d9327f71b09e");
  REQUIRE(guid.has_value());
  REQUIRE(caors::meta::type_<duplicate_guid_owner_type>("caors.test.type.guid_owner")
              .guid(caors::type_guid{*guid})
              .result() == caors::error{});

  CHECK(caors::meta::type_<duplicate_guid_type>("caors.test.type.duplicate_guid")
            .guid(caors::type_guid{*guid})
            .result() == meta_error(caors::errc::duplicate_guid));
}

TEST_CASE("meta type - global registry checks parent relationship", "[meta]") {
  REQUIRE(caors::meta::type_<relation_parent_type>("caors.test.type.relation_parent").result() ==
          caors::error{});
  const auto parent = caors::meta::find("caors.test.type.relation_parent");
  REQUIRE(parent);

  REQUIRE(caors::meta::type_<relation_child_type>("caors.test.type.relation_child")
              .parent(parent.id())
              .result() == caors::error{});
  const auto child = caors::meta::find("caors.test.type.relation_child");
  REQUIRE(child);

  CHECK(caors::meta::get_registry().contains(parent.id()));
  CHECK(caors::meta::get_registry().contains(child.id()));
  CHECK(caors::meta::is_a(child.id(), child.id()));
  CHECK(caors::meta::is_a(child.id(), parent.id()));
  CHECK_FALSE(caors::meta::is_a(parent.id(), child.id()));
  CHECK(child.parent().id() == parent.id());
}
