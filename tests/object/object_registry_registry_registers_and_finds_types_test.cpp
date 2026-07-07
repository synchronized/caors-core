// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - registry registers and finds types", "[object]") {
  CHECK(register_object_test_type<registry_actor>() == caors::error{});
  CHECK(register_object_test_type<registry_actor>() == object_error(caors::errc::duplicate_id));

  auto type = caors::objects::find<registry_actor>();
  REQUIRE(type);
  CHECK(type.id() == registry_actor::static_type_id());
  CHECK(type.name() == registry_actor::object_type_name);
  CHECK_FALSE(type.guid());
  CHECK(type.size() == sizeof(registry_actor));
  CHECK(type.alignment() == alignof(registry_actor));
  CHECK(type.constructible());

  auto by_name = caors::objects::find(registry_actor::object_type_name);
  REQUIRE(by_name);
  CHECK(by_name.id() == registry_actor::static_type_id());
}
