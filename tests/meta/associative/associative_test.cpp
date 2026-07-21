// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：meta_associative_associative_view_exposes_key_and_value_metadata_test.cpp

#include "meta_property_test_support.h"

#include "caors/meta/value/associative_entry_ref.h"
#include "caors/meta/value/capabilities.h"

using namespace test_meta;

TEST_CASE("meta - associative property view exposes key and value metadata", "[meta]") {
  REQUIRE(caors::registration::type_<hash_map_holder>("caors.test.meta.hash_map_holder")
              .meta()
              .property("values", &hash_map_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::meta::find("caors.test.meta.hash_map_holder");
  REQUIRE(reflected);

  auto values = reflected.find_property("values");
  REQUIRE(values);
  CHECK(values.kind() == caors::meta::type_kind::associative);
  CHECK(values.is_associative());
  CHECK(values.get_type().name() == "caors.hash_map<int,int>");
  CHECK(values.get_key_type().id() == registered_type_id("int"));
  CHECK(values.get_key_type().name() == "int");
  CHECK(values.get_value_type().id() == registered_type_id("int"));
  CHECK(values.get_mapped_type().id() == registered_type_id("int"));
  CHECK(values.get_mapped_type().name() == "int");
}

// 原测试文件：meta_associative_associative_view_supports_dynamic_operations_test.cpp

#include "meta_property_test_support.h"

#include "caors/meta/value/associative_entry_ref.h"
#include "caors/meta/value/capabilities.h"

using namespace test_meta;

TEST_CASE("meta - associative property view supports dynamic operations", "[meta]") {
  REQUIRE(caors::registration::type_<hash_map_ops_holder>("caors.test.meta.hash_map_ops_holder")
              .meta()
              .property("values", &hash_map_ops_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::meta::find("caors.test.meta.hash_map_ops_holder");
  REQUIRE(reflected);

  auto values = reflected.find_property("values");
  REQUIRE(values);

  hash_map_ops_holder instance{};
  caors::meta::associative_view map = values.get_ref(caors::meta::view(instance))
                                          .get_capability<caors::meta::associative_capability>();
  CHECK(map.valid());
  CHECK(map.writable());
  CHECK(map.get_key_type().id() == registered_type_id("int"));
  CHECK(map.get_key_type().name() == "int");
  CHECK(map.get_value_type().id() == registered_type_id("int"));
  CHECK(map.get_value_type().name() == "int");
  CHECK(map.get_mapped_type().id() == registered_type_id("int"));
  CHECK(map.get_mapped_type().name() == "int");

  CHECK(map.insert_value(caors::any::from(7), caors::any::from(70)));
  CHECK_FALSE(map.insert_value(caors::any::from(7), caors::any::from(700)));
  CHECK_FALSE(map.insert_value(caors::any::from(8.0F), caors::any::from(80)));

  int key_sum = 0;
  int value_sum = 0;
  for (caors::meta::associative_entry_ref entry : map) {
    const int* key = entry.key().try_get<int>();
    const int* value = entry.value().try_get<int>();
    REQUIRE(key != nullptr);
    REQUIRE(value != nullptr);
    key_sum += *key;
    value_sum += *value;
  }
  CHECK(key_sum == 7);
  CHECK(value_sum == 70);

  const std::optional<std::size_t> size = map.size();
  REQUIRE(size.has_value());
  CHECK(*size == 1);

  const hash_map_ops_holder& readonly_instance = instance;
  caors::meta::associative_view readonly_map =
      values.get_ref(caors::meta::view_const(readonly_instance))
          .get_capability<caors::meta::associative_capability>();
  CHECK(readonly_map.valid());
  CHECK_FALSE(readonly_map.writable());
  CHECK_FALSE(readonly_map.insert_value(caors::any::from(8), caors::any::from(80)));
  CHECK_FALSE(readonly_map.clear());

  int readonly_key = 0;
  int readonly_mapped = 0;
  int readonly_count = 0;
  for (caors::meta::associative_entry_ref entry : readonly_map) {
    const int* key = entry.key().try_get<int>();
    const int* mapped = entry.value().try_get<int>();
    REQUIRE(key != nullptr);
    REQUIRE(mapped != nullptr);
    readonly_key = *key;
    readonly_mapped = *mapped;
    ++readonly_count;
  }

  CHECK(readonly_count == 1);
  CHECK(readonly_key == 7);
  CHECK(readonly_mapped == 70);

  CHECK(map.clear());
  const std::optional<std::size_t> empty_size = map.size();
  REQUIRE(empty_size.has_value());
  CHECK(*empty_size == 0);
  CHECK(instance.values.empty());
}

// 原测试文件：meta_associative_set_view_exposes_element_metadata_test.cpp

#include "meta_property_test_support.h"

#include "caors/meta/value/associative_entry_ref.h"
#include "caors/meta/value/capabilities.h"

using namespace test_meta;

TEST_CASE("meta - set property view exposes element metadata", "[meta]") {
  REQUIRE(caors::registration::type_<hash_set_holder>("caors.test.meta.hash_set_holder")
              .meta()
              .property("values", &hash_set_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::meta::find("caors.test.meta.hash_set_holder");
  REQUIRE(reflected);

  auto values = reflected.find_property("values");
  REQUIRE(values);
  CHECK(values.kind() == caors::meta::type_kind::set);
  CHECK(values.is_set());
  CHECK(values.get_type().name() == "caors.hash_set<int>");
  CHECK(values.get_value_type().id() == registered_type_id("int"));
  CHECK(values.get_element_type().id() == registered_type_id("int"));
  CHECK(values.get_element_type().name() == "int");

  hash_set_holder instance{};
  caors::meta::set_view set =
      values.get_ref(caors::meta::view(instance)).get_capability<caors::meta::set_capability>();
  CHECK(set.valid());
  CHECK(set.writable());
  CHECK(set.get_value_type().id() == registered_type_id("int"));
  CHECK(set.get_value_type().name() == "int");
  CHECK(set.get_element_type().id() == registered_type_id("int"));
  CHECK(set.get_element_type().name() == "int");
  CHECK(set.insert_value(caors::any::from(11)));
  CHECK_FALSE(set.insert_value(caors::any::from(11)));
  CHECK_FALSE(set.insert_value(caors::any::from(2.0F)));

  int set_sum = 0;
  for (caors::any_ref value : set) {
    const int* typed = value.try_get<int>();
    REQUIRE(typed != nullptr);
    set_sum += *typed;
  }
  CHECK(set_sum == 11);

  const std::optional<std::size_t> size = set.size();
  REQUIRE(size.has_value());
  CHECK(*size == 1);

  const hash_set_holder& readonly_instance = instance;
  caors::meta::set_view readonly_set = values.get_ref(caors::meta::view_const(readonly_instance))
                                           .get_capability<caors::meta::set_capability>();
  CHECK(readonly_set.valid());
  CHECK_FALSE(readonly_set.writable());
  CHECK_FALSE(readonly_set.insert_value(caors::any::from(12)));
  CHECK_FALSE(readonly_set.clear());

  int visited = 0;
  for (caors::any_ref element : readonly_set) {
    const int* value = element.try_get<int>();
    REQUIRE(value != nullptr);
    visited = *value;
  }
  CHECK(visited == 11);

  CHECK(set.clear());
  CHECK(instance.values.empty());
}
