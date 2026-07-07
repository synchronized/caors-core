// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "value_test_common.h"

using namespace caors::tests::reflection;

TEST_CASE("reflection any_ref - creates variant views for registered variant values",
          "[reflection]") {
  require_value_test_meta();

  std::variant<int, caors::string> value{3};

  caors::any_ref reference = caors::any_ref::from(value, "std.variant<int,caors.string>");
  REQUIRE(reference.valid());

  caors::value::variant_view variant = reference.create_variant_view();
  REQUIRE(variant.valid());
  CHECK(variant.writable());
  CHECK(variant.size() == 2);
  REQUIRE(variant.index().has_value());
  CHECK(*variant.index() == 0);
  REQUIRE(variant.alternative_type(1).has_value());
  CHECK(*variant.alternative_type(1) == caors::meta::id("caors.string"));

  caors::any_ref active = variant.value_ref();
  REQUIRE(active.valid());
  CHECK(active.try_set(7));
  CHECK(std::get<int>(value) == 7);

  REQUIRE(variant.emplace(1));
  CHECK(variant.value_type_name() == "caors.string");
  active = variant.value_ref();
  REQUIRE(active.valid());
  CHECK(active.try_set(caors::string{"seven"}));
  CHECK(std::get<caors::string>(value) == "seven");
}
