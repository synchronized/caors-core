// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"
#include "caors/meta.h"
#include "caors/traits/type_name.h"
#include "caors/value.h"

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

struct guid_sample_type {};

struct duplicate_guid_owner_type {};

struct relation_parent_type {};

struct relation_child_type : relation_parent_type {};

struct lazy_meta_builder_type {};

struct fluent_meta_type {};

struct stable_builder_type {};

struct explicit_find_type {};

struct missing_find_type {};

struct default_factory_type {
  int value = 11;
};

struct missing_default_factory_type {
  explicit missing_default_factory_type(int input) : value(input) {}

  int value = 0;
};

struct custom_factory_type {
  explicit custom_factory_type(int input) : value(input) {}

  int value = 0;
};

[[nodiscard]] constexpr caors::error meta_error(caors::errc code) noexcept {
  return {caors::error_domain::meta, code};
}

void* construct_custom_factory_type(caors::memory_resource* resource) {
  if (resource != nullptr) {
    return caors::memory::make<custom_factory_type>(resource, 23);
  }

  return new custom_factory_type(23);
}

void destroy_custom_factory_type(caors::memory_resource* resource, void* value) noexcept {
  auto* typed = static_cast<custom_factory_type*>(value);
  if (resource != nullptr) {
    caors::memory::destroy(typed, resource);
    return;
  }

  delete typed;
}

}

TEST_CASE("meta type - built-in C++ types have default names", "[meta]") {
  CHECK(caors::traits::type_name<int>::value == "int");
  CHECK(caors::traits::type_name<float>::value == "float");
  CHECK(caors::traits::type_name<bool>::value == "bool");
}

TEST_CASE("meta type - type id is allocated by explicit registration", "[meta]") {
  CHECK_FALSE(caors::meta::id("caors.test.type.unregistered_name"));
  CHECK_FALSE(caors::meta::id_of<anonymous_cpp_type>());
  CHECK_FALSE(caors::meta::find<anonymous_cpp_type>());

  REQUIRE(caors::meta::type_<anonymous_cpp_type>("caors.test.type.anonymous_cpp").result() ==
          caors::error{});

  const caors::type_id first = caors::meta::id_of<anonymous_cpp_type>();
  const caors::type_id second = caors::meta::id_of<anonymous_cpp_type>();

  CHECK(first);
  CHECK(first == second);

  auto named = caors::meta::find("caors.test.type.anonymous_cpp");
  REQUIRE(named);
  CHECK(named.id() == first);
  CHECK(caors::meta::find<anonymous_cpp_type>().id() == first);
  CHECK(caors::any::default_construct(named.id()).valid());
  CHECK(caors::any::from(anonymous_cpp_type{}, "caors.test.type.anonymous_cpp").valid());
  CHECK(caors::meta::id("caors.test.type.anonymous_cpp") == first);
}

TEST_CASE("meta type view - describes layout", "[meta]") {
  constexpr std::string_view name = "caors.test.sample";
  const caors::meta::type type = caors::meta::type_<sample_type>(name).get();

  CHECK(type.valid());
  CHECK(type.id());
  CHECK(type.name() == name);
  CHECK_FALSE(type.guid());
  CHECK(type.size() == sizeof(sample_type));
  CHECK(type.alignment() == alignof(sample_type));
  CHECK_FALSE(type.parent_id());

  caors::any value = caors::any::default_construct(type.id());
  REQUIRE(value.valid());
  REQUIRE(value.try_get_mutable<sample_type>() != nullptr);
  CHECK(value.try_set(sample_type{7}));
  REQUIRE(value.try_get<sample_type>() != nullptr);
  CHECK(value.try_get<sample_type>()->value == 7);
}

TEST_CASE("meta type view - describes stable type guid", "[meta]") {
  const auto guid = caors::guid::parse("5ed39a97-7b20-4d87-86ac-e62a4a615b10");
  REQUIRE(guid.has_value());

  const caors::meta::type type = caors::meta::type_<guid_sample_type>("caors.test.guid.sample")
                                     .guid(caors::type_guid{*guid})
                                     .get();

  CHECK(type.valid());
  CHECK(type.id());
  CHECK(type.guid() == caors::type_guid{*guid});
  CHECK(type.guid().value.string() == "5ed39a97-7b20-4d87-86ac-e62a4a615b10");
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

TEST_CASE("meta type - fluent builder registers name and guid", "[meta]") {
  const auto guid = caors::guid::parse("d2953377-4ffc-4c81-aeef-7e020272a536");
  REQUIRE(guid.has_value());

  auto builder = caors::meta::type_<fluent_meta_type>()
                     .type("caors.test.type.fluent_meta")
                     .guid(caors::type_guid{*guid});

  CHECK(builder.succeeded());
  const caors::meta::type type = builder.get();
  REQUIRE(type);
  CHECK(type.name() == "caors.test.type.fluent_meta");
  CHECK(type.guid() == caors::type_guid{*guid});
  CHECK(caors::meta::find<fluent_meta_type>().id() == type.id());
  CHECK(caors::meta::find(caors::type_guid{*guid}).id() == type.id());
}

TEST_CASE("meta type - builder updates registered data in place", "[meta]") {
  const auto guid = caors::guid::parse("2f7cc996-e0bc-4b61-85ea-b0263b876081");
  REQUIRE(guid.has_value());

  auto builder = caors::meta::type_<stable_builder_type>().type("caors.test.type.stable_builder");
  const caors::meta::type before = builder.get();
  REQUIRE(before);
  CHECK_FALSE(before.guid());
  CHECK(before.size() == sizeof(stable_builder_type));
  CHECK(before.alignment() == alignof(stable_builder_type));

  REQUIRE(builder.guid(caors::type_guid{*guid}).result() == caors::error{});
  const caors::meta::type after = builder.get();
  REQUIRE(after);

  CHECK(before.id() == after.id());
  CHECK(after.size() == sizeof(stable_builder_type));
  CHECK(after.alignment() == alignof(stable_builder_type));
  CHECK(before.guid() == caors::type_guid{*guid});
  CHECK(after.guid() == caors::type_guid{*guid});
}

TEST_CASE("meta type - builder without name does not register type", "[meta]") {
  const auto builder = caors::meta::type_<lazy_meta_builder_type>();

  CHECK_FALSE(builder.get());
  CHECK_FALSE(caors::meta::find<lazy_meta_builder_type>());
  CHECK_FALSE(caors::meta::find<lazy_meta_builder_type>());
}

TEST_CASE("meta type - explicit type access does not create missing types", "[meta]") {
  CHECK_FALSE(caors::meta::find<missing_find_type>());

  const caors::meta::type type =
      caors::meta::type_<explicit_find_type>("caors.test.type.explicit_find").get();
  REQUIRE(type.valid());

  const caors::meta::type found = caors::meta::find<explicit_find_type>();
  REQUIRE(found.valid());
  CHECK(found.id() == type.id());
}

TEST_CASE("meta type - factory registers resource-aware default construction", "[meta]") {
  auto builder =
      caors::meta::type_<default_factory_type>("caors.test.type.factory_default").factory();
  CHECK(builder.succeeded());

  const caors::meta::type type = builder.get();
  REQUIRE(type.valid());

  const caors::meta::detail::value_factory_ops factory =
      caors::meta::get_registry().find_type_factory(type.id());
  REQUIRE(factory.valid());

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  auto* value = static_cast<default_factory_type*>(factory.construct(&tracking));
  REQUIRE(value != nullptr);
  CHECK(value->value == 11);
  CHECK(tracking.statistics().allocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use >= sizeof(default_factory_type));

  factory.destroy(&tracking, value);
  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("meta type - default factory rejects non-default-constructible types", "[meta]") {
  auto builder =
      caors::meta::type_<missing_default_factory_type>("caors.test.type.factory_missing_default")
          .factory();

  CHECK(builder.result() == meta_error(caors::errc::invalid_type));
  REQUIRE(builder.get().valid());
  CHECK_FALSE(caors::meta::get_registry().find_type_factory(builder.get().id()).valid());
}

TEST_CASE("meta type - factory registers custom construction strategy", "[meta]") {
  auto builder = caors::meta::type_<custom_factory_type>("caors.test.type.factory_custom")
                     .factory(&construct_custom_factory_type, &destroy_custom_factory_type);
  CHECK(builder.succeeded());

  const caors::meta::detail::value_factory_ops factory =
      caors::meta::get_registry().find_type_factory(builder.get().id());
  REQUIRE(factory.valid());

  auto* value = static_cast<custom_factory_type*>(factory.construct(nullptr));
  REQUIRE(value != nullptr);
  CHECK(value->value == 23);
  factory.destroy(nullptr, value);

  auto duplicate = caors::meta::type_<custom_factory_type>("caors.test.type.factory_custom")
                       .factory(&construct_custom_factory_type, &destroy_custom_factory_type);
  CHECK(duplicate.result() == meta_error(caors::errc::duplicate_id));
  CHECK(duplicate.succeeded());
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
