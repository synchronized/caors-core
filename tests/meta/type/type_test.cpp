// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：meta_type_basic_meta_type_built_in_c_types_have_default_names_test.cpp

#include "meta_type_basic_test_support.h"

using namespace caors::tests::meta_type_basic;
TEST_CASE("meta type - built-in C++ types have default names", "[meta]") {
  CHECK(caors::traits::type_name<int>::value == "int");
  CHECK(caors::traits::type_name<float>::value == "float");
  CHECK(caors::traits::type_name<bool>::value == "bool");
}

// 原测试文件：meta_type_basic_meta_type_type_id_is_allocated_by_explicit_registration_test.cpp

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

// 原测试文件：meta_type_basic_meta_type_view_describes_layout_test.cpp

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

// 原测试文件：meta_type_basic_meta_type_view_describes_stable_type_guid_test.cpp

#include "meta_type_basic_test_support.h"

using namespace caors::tests::meta_type_basic;
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
