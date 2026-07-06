// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <catch2/catch_all.hpp>

namespace {

struct lazy_meta_builder_type {};

struct fluent_meta_type {};

struct stable_builder_type {};

struct explicit_find_type {};

struct missing_find_type {};

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
