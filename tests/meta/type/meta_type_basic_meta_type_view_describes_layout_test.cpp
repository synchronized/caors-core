// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_type_basic_test_support.h"

using namespace caors::tests::meta_type_basic;
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
