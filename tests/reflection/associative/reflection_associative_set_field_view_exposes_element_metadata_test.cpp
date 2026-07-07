// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

#include "caors/value/associative_entry_ref.h"
#include "caors/value/view_factory.h"

using namespace test_reflection;

TEST_CASE("reflection - set field view exposes element metadata", "[reflection]") {
  REQUIRE(caors::reflection::class_<hash_set_holder>("caors.test.reflection.hash_set_holder")
              .field("values", &hash_set_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.hash_set_holder");
  REQUIRE(reflected);

  auto values = reflected.find_field("values");
  REQUIRE(values);
  CHECK(values.kind() == caors::meta::type_kind::set);
  CHECK(values.is_set());
  CHECK(values.type_name() == "caors.hash_set<int>");
  CHECK(values.value_type() == registered_type_id("int"));
  CHECK(values.element_type() == registered_type_id("int"));
  CHECK(values.element_type_name() == "int");

  hash_set_holder instance{};
  caors::value::set_view set = values.get_ref(&instance).create_set_view();
  CHECK(set.valid());
  CHECK(set.writable());
  CHECK(set.value_type() == registered_type_id("int"));
  CHECK(set.value_type_name() == "int");
  CHECK(set.element_type() == registered_type_id("int"));
  CHECK(set.element_type_name() == "int");
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
  caors::value::set_view readonly_set = values.get_ref(&readonly_instance).create_set_view();
  CHECK(readonly_set.valid());
  CHECK_FALSE(readonly_set.writable());
  CHECK_FALSE(readonly_set.insert_value(caors::any::from(12)));
  CHECK_FALSE(readonly_set.clear());

  int visited = 0;
  CHECK(readonly_set.for_each_ref(
      [](caors::any_ref element, void* user_data) {
        auto* visited = static_cast<int*>(user_data);
        const int* value = element.try_get<int>();
        if (value == nullptr) {
          return false;
        }

        *visited = *value;
        return true;
      },
      &visited));
  CHECK(visited == 11);

  CHECK(set.clear());
  CHECK(instance.values.empty());
}
