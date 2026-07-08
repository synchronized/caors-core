// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 鍘熸祴璇曟枃浠讹細meta_type_builder_meta_type_builder_updates_registered_data_in_place_test.cpp

#include "meta_type_builder_test_support.h"

using namespace caors::tests::meta_type_builder;
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

// 鍘熸祴璇曟枃浠讹細meta_type_builder_meta_type_builder_without_name_does_not_register_type_test.cpp

#include "meta_type_builder_test_support.h"

using namespace caors::tests::meta_type_builder;
TEST_CASE("meta type - builder without name does not register type", "[meta]") {
  const auto builder = caors::meta::type_<lazy_meta_builder_type>();

  CHECK_FALSE(builder.get());
  CHECK(caors::meta::find<lazy_meta_builder_type>());
  CHECK_FALSE(caors::meta::find<lazy_meta_builder_type>().registered());
}

// 鍘熸祴璇曟枃浠讹細meta_type_builder_meta_type_explicit_type_access_does_not_create_missing_types_test.cpp

#include "meta_type_builder_test_support.h"

using namespace caors::tests::meta_type_builder;
TEST_CASE("meta type - explicit type access creates descriptor-only views", "[meta]") {
  const caors::meta::type missing = caors::meta::find<missing_find_type>();
  REQUIRE(missing);
  CHECK_FALSE(missing.registered());

  const caors::meta::type type =
      caors::meta::type_<explicit_find_type>("caors.test.type.explicit_find").get();
  REQUIRE(type.valid());
  CHECK(type.registered());

  const caors::meta::type found = caors::meta::find<explicit_find_type>();
  REQUIRE(found.valid());
  CHECK(found.registered());
  CHECK(found.id() == type.id());
}

// 鍘熸祴璇曟枃浠讹細meta_type_builder_meta_type_fluent_builder_registers_name_and_guid_test.cpp

#include "meta_type_builder_test_support.h"

using namespace caors::tests::meta_type_builder;
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
