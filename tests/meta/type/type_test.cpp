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
TEST_CASE("meta type - descriptor id is available before explicit registration", "[meta]") {
  CHECK_FALSE(caors::meta::id("caors.test.type.unregistered_name"));
  const caors::meta::type descriptor = caors::meta::resolve<anonymous_cpp_type>();
  REQUIRE(descriptor);
  CHECK_FALSE(descriptor.registered());
  CHECK_FALSE(caors::meta::find<anonymous_cpp_type>());
  CHECK(caors::meta::id_of<anonymous_cpp_type>() == descriptor.id());
  const caors::any descriptor_value = caors::any::default_construct(descriptor);
  REQUIRE(descriptor_value.valid());
  CHECK(descriptor_value.size() == sizeof(anonymous_cpp_type));
  CHECK(descriptor_value.alignment() == alignof(anonymous_cpp_type));

  REQUIRE(
      caors::registration::type_<anonymous_cpp_type>("caors.test.type.anonymous_cpp").commit() ==
      caors::error{});

  const caors::type_id first = caors::meta::id_of<anonymous_cpp_type>();
  const caors::type_id second = caors::meta::id_of<anonymous_cpp_type>();

  CHECK(first);
  CHECK(first == second);
  CHECK(first == descriptor.id());

  auto named = caors::meta::find("caors.test.type.anonymous_cpp");
  REQUIRE(named);
  CHECK(named.registered());
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
  REQUIRE(caors::registration::type_<sample_type>(name).commit() == caors::error{});
  const caors::meta::type type = caors::meta::find<sample_type>();

  CHECK(type.valid());
  CHECK(type.id());
  CHECK(type.name() == name);
  CHECK_FALSE(type.guid());
  const caors::any type_value = caors::any::default_construct(type);
  REQUIRE(type_value.valid());
  CHECK(type_value.size() == sizeof(sample_type));
  CHECK(type_value.alignment() == alignof(sample_type));
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

  REQUIRE(caors::registration::type_<guid_sample_type>("caors.test.guid.sample")
              .guid(caors::type_guid{*guid})
              .commit() == caors::error{});
  const caors::meta::type type = caors::meta::find<guid_sample_type>();

  CHECK(type.valid());
  CHECK(type.id());
  CHECK(type.guid() == caors::type_guid{*guid});
  CHECK(type.guid().value.string() == "5ed39a97-7b20-4d87-86ac-e62a4a615b10");
}

enum class registered_enum_type {
  first = 1,
  second = 2,
};

TEST_CASE("meta enum - enum builder registers enum type", "[meta]") {
  REQUIRE(caors::registration::enum_<registered_enum_type>("caors.test.enum.registered").commit() ==
          caors::error{});
  const caors::meta::type type = caors::meta::find<registered_enum_type>();

  REQUIRE(type.valid());
  CHECK(type.registered());
  CHECK(type.kind() == caors::meta::type_kind::enumeration);
  CHECK(type.name() == "caors.test.enum.registered");
}

TEST_CASE("meta type - registration records nested type relationships", "[meta]") {
  REQUIRE(caors::registration::type_<nested_inner_type>("caors.test.type.nested_inner").commit() ==
          caors::error{});
  REQUIRE(caors::registration::enum_<nested_kind_type>("caors.test.type.nested_kind").commit() ==
          caors::error{});
  const caors::meta::type kind = caors::meta::find<nested_kind_type>();
  REQUIRE(kind);

  REQUIRE(caors::registration::type_<nested_owner_type>("caors.test.type.nested_owner")
              .meta()
              .nested<nested_inner_type>("inner")
              .nested("kind", kind)
              .commit() == caors::error{});

  const caors::meta::type owner = caors::meta::find<nested_owner_type>();
  REQUIRE(owner);
  CHECK(owner.nested_count() == 2);
  CHECK(owner.nested_name_at(0) == "inner");
  CHECK(owner.nested_at(0).id() == caors::meta::find<nested_inner_type>().id());
  CHECK(owner.find_nested("inner").name() == "caors.test.type.nested_inner");
  CHECK(owner.find_nested("kind").id() == kind.id());
  CHECK_FALSE(owner.find_nested("missing"));
  CHECK_FALSE(owner.nested_at(99));
  CHECK_FALSE(owner.nested_name_at(99));
}

TEST_CASE("meta type - registration rejects invalid nested type relationships", "[meta]") {
  REQUIRE(caors::registration::type_<nested_duplicate_name_type>(
              "caors.test.type.nested_duplicate_name")
              .commit() == caors::error{});
  REQUIRE(
      caors::registration::type_<nested_duplicate_id_type>("caors.test.type.nested_duplicate_id")
          .commit() == caors::error{});

  CHECK(caors::registration::type_<nested_duplicate_name_owner_type>(
            "caors.test.type.nested_owner_duplicate")
            .meta()
            .nested<nested_duplicate_name_type>("same")
            .nested<nested_duplicate_id_type>("same")
            .commit() == caors::error{caors::error_domain::meta, caors::errc::duplicate_name});

  CHECK(caors::registration::type_<nested_duplicate_id_owner_type>(
            "caors.test.type.nested_owner_duplicate_id")
            .meta()
            .nested<nested_duplicate_name_type>("first")
            .nested<nested_duplicate_name_type>("second")
            .commit() == caors::error{caors::error_domain::meta, caors::errc::duplicate_id});

  CHECK(
      caors::registration::type_<nested_missing_owner_type>("caors.test.type.nested_owner_missing")
          .meta()
          .nested<nested_missing_type>("missing")
          .commit() == caors::error{caors::error_domain::meta, caors::errc::missing_type});
}
