// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

#include "caors/value/associative_entry_ref.h"
#include "caors/value/view_factory.h"

using namespace test_reflection;

TEST_CASE("reflection - associative field view supports dynamic operations", "[reflection]") {
  REQUIRE(
      caors::reflection::class_<hash_map_ops_holder>("caors.test.reflection.hash_map_ops_holder")
          .field("values", &hash_map_ops_holder::values)
          .commit()
          .succeeded());

  auto reflected = caors::reflection::find("caors.test.reflection.hash_map_ops_holder");
  REQUIRE(reflected);

  auto values = reflected.find_field("values");
  REQUIRE(values);

  hash_map_ops_holder instance{};
  caors::value::associative_view map = values.get_ref(&instance).create_associative_view();
  CHECK(map.valid());
  CHECK(map.writable());
  CHECK(map.key_type() == registered_type_id("int"));
  CHECK(map.key_type_name() == "int");
  CHECK(map.value_type() == registered_type_id("int"));
  CHECK(map.value_type_name() == "int");
  CHECK(map.mapped_type() == registered_type_id("int"));
  CHECK(map.mapped_type_name() == "int");

  CHECK(map.insert_value(caors::any::from(7), caors::any::from(70)));
  CHECK_FALSE(map.insert_value(caors::any::from(7), caors::any::from(700)));
  CHECK_FALSE(map.insert_value(caors::any::from(8.0F), caors::any::from(80)));

  int key_sum = 0;
  int value_sum = 0;
  for (caors::value::associative_entry_ref entry : map) {
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
  caors::value::associative_view readonly_map =
      values.get_ref(&readonly_instance).create_associative_view();
  CHECK(readonly_map.valid());
  CHECK_FALSE(readonly_map.writable());
  CHECK_FALSE(readonly_map.insert_value(caors::any::from(8), caors::any::from(80)));
  CHECK_FALSE(readonly_map.clear());

  struct visit_state {
    int key = 0;
    int mapped = 0;
    int count = 0;
  };

  visit_state state{};
  CHECK(readonly_map.for_each_ref(
      [](caors::any_ref key, caors::any_ref mapped, void* user_data) {
        auto* state = static_cast<visit_state*>(user_data);
        const int* key_value = key.try_get<int>();
        const int* mapped_value = mapped.try_get<int>();
        if (key_value == nullptr || mapped_value == nullptr) {
          return false;
        }

        state->key = *key_value;
        state->mapped = *mapped_value;
        ++state->count;
        return true;
      },
      &state));

  CHECK(state.count == 1);
  CHECK(state.key == 7);
  CHECK(state.mapped == 70);

  CHECK(map.clear());
  const std::optional<std::size_t> empty_size = map.size();
  REQUIRE(empty_size.has_value());
  CHECK(*empty_size == 0);
  CHECK(instance.values.empty());
}
