// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_type_basic_test_support.h"

using namespace caors::tests::meta_type_basic;
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
