// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：meta_type_registry_meta_type_global_registry_checks_parent_relationship_test.cpp

#include "meta_type_registry_test_support.h"

using namespace caors::tests::meta_type_registry;
TEST_CASE("meta type - global registry checks parent relationship", "[meta]") {
  REQUIRE(caors::registration::type_<relation_parent_type>("caors.test.type.relation_parent")
              .commit() == caors::error{});
  const auto parent = caors::meta::find("caors.test.type.relation_parent");
  REQUIRE(parent);

  REQUIRE(caors::registration::type_<relation_child_type>("caors.test.type.relation_child")
              .parent(parent.id())
              .commit() == caors::error{});
  const auto child = caors::meta::find("caors.test.type.relation_child");
  REQUIRE(child);

  CHECK(caors::meta::get_registry().contains(parent.id()));
  CHECK(caors::meta::get_registry().contains(child.id()));
  CHECK(caors::meta::is_a(child.id(), child.id()));
  CHECK(caors::meta::is_a(child.id(), parent.id()));
  CHECK_FALSE(caors::meta::is_a(parent.id(), child.id()));
  CHECK(child.parent().id() == parent.id());
}

// 原测试文件：meta_type_registry_meta_type_global_registry_finds_types_by_c_type_test.cpp

#include "meta_type_registry_test_support.h"

using namespace caors::tests::meta_type_registry;
TEST_CASE("meta type - global registry finds types by C++ type", "[meta]") {
  REQUIRE(caors::registration::type_<cpp_sample_type>("caors.test.type.cpp_sample").commit() ==
          caors::error{});

  auto type = caors::meta::find<cpp_sample_type>();
  REQUIRE(type);
  CHECK(type.name() == "caors.test.type.cpp_sample");
  CHECK(caors::meta::id("caors.test.type.cpp_sample") == type.id());
}

// 原测试文件：meta_type_registry_meta_type_global_registry_finds_types_by_stable_guid_test.cpp

#include "meta_type_registry_test_support.h"

using namespace caors::tests::meta_type_registry;
TEST_CASE("meta type - global registry finds types by stable guid", "[meta]") {
  const auto guid = caors::guid::parse("7b458821-d56a-4133-a5a6-90a8661ee474");
  REQUIRE(guid.has_value());

  REQUIRE(caors::registration::type_<guid_parent_type>("caors.test.type.guid_parent")
              .guid(caors::type_guid{*guid})
              .commit() == caors::error{});

  auto type = caors::meta::find(caors::type_guid{*guid});
  REQUIRE(type);
  CHECK(type.name() == "caors.test.type.guid_parent");
  CHECK(type.guid() == caors::type_guid{*guid});
}

// 原测试文件：meta_type_registry_meta_type_global_registry_registers_and_finds_types_test.cpp

#include "meta_type_registry_test_support.h"

using namespace caors::tests::meta_type_registry;
TEST_CASE("meta type - global registry registers and finds types", "[meta]") {
  CHECK(caors::registration::type_<registry_parent_type>("caors.test.type.registry_parent")
            .commit() == caors::error{});
  CHECK(caors::registration::type_<registry_parent_type>("caors.test.type.registry_parent")
            .commit() == meta_error(caors::errc::duplicate_id));

  auto type = caors::meta::find("caors.test.type.registry_parent");
  REQUIRE(type);
  CHECK(type.id());
  CHECK(type.name() == "caors.test.type.registry_parent");
  const caors::any value = caors::any::default_construct(type);
  REQUIRE(value.valid());
  CHECK(value.size() == sizeof(registry_parent_type));
  CHECK(value.alignment() == alignof(registry_parent_type));

  auto by_name = caors::meta::find("caors.test.type.registry_parent");
  REQUIRE(by_name);
  CHECK(by_name.id() == type.id());
}

// 原测试文件：meta_type_registry_meta_type_global_registry_reports_registration_errors_test.cpp

#include "meta_type_registry_test_support.h"

using namespace caors::tests::meta_type_registry;
TEST_CASE("meta type - global registry reports registration errors", "[meta]") {
  CHECK(caors::registration::type_<error_child_type>("caors.test.type.missing_parent_child")
            .parent(caors::type_id{std::numeric_limits<std::uint64_t>::max()})
            .commit() == meta_error(caors::errc::missing_parent));

  REQUIRE(caors::registration::type_<error_parent_type>("caors.test.type.error_parent").commit() ==
          caors::error{});

  CHECK(caors::registration::type_<error_child_type>("caors.test.type.error_parent").commit() ==
        meta_error(caors::errc::duplicate_name));

  const auto guid = caors::guid::parse("c9e7a3da-2cc0-4048-97ef-d9327f71b09e");
  REQUIRE(guid.has_value());
  REQUIRE(caors::registration::type_<duplicate_guid_owner_type>("caors.test.type.guid_owner")
              .guid(caors::type_guid{*guid})
              .commit() == caors::error{});

  CHECK(caors::registration::type_<duplicate_guid_type>("caors.test.type.duplicate_guid")
            .guid(caors::type_guid{*guid})
            .commit() == meta_error(caors::errc::duplicate_guid));
}
