// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

#include "caors/value/associative_entry_ref.h"
#include "caors/value/view_factory.h"

using namespace test_reflection;

TEST_CASE("reflection - container views can be created from any_ref", "[reflection]") {
  caors::vector<int> scores{};
  caors::any_ref scores_ref = caors::any_ref::from(scores);
  REQUIRE(scores_ref.valid());

  caors::value::sequence_view sequence = scores_ref.create_sequence_view();
  CHECK(sequence.valid());
  CHECK(sequence.writable());
  CHECK(sequence.append_value(caors::any::from(10)));
  CHECK(sequence.append_value(caors::any::from(20)));
  CHECK(scores.size() == 2);
  caors::any_ref second_score = sequence.element_ref(1);
  REQUIRE(second_score.valid());
  CHECK(second_score.try_set(25));
  CHECK(scores[1] == 25);

  caors::hash_set<int> ids{};
  caors::any_ref ids_ref = caors::any_ref::from(ids);
  REQUIRE(ids_ref.valid());

  caors::value::set_view set = ids_ref.create_set_view();
  CHECK(set.valid());
  CHECK(set.writable());
  CHECK(set.insert_value(caors::any::from(3)));
  CHECK(ids.contains(3));

  caors::hash_map<int, int> values{};
  caors::any_ref ref = caors::any_ref::from(values);
  REQUIRE(ref.valid());

  caors::value::associative_view map = ref.create_associative_view();
  CHECK(map.valid());
  CHECK(map.writable());
  CHECK(map.insert_value(caors::any::from(3), caors::any::from(30)));
  CHECK(values.at(3) == 30);

  const caors::any_ref readonly_ref = caors::any_ref::from_const(values);
  caors::value::associative_view readonly_map = readonly_ref.create_associative_view();
  CHECK(readonly_map.valid());
  CHECK_FALSE(readonly_map.writable());
  CHECK_FALSE(readonly_map.insert_value(caors::any::from(4), caors::any::from(40)));

  caors::any copied = caors::any::from(values);
  caors::value::associative_view copied_map = copied.create_associative_view();
  CHECK(copied_map.valid());
  CHECK(copied_map.writable());
  CHECK(copied_map.insert_value(caors::any::from(5), caors::any::from(50)));

  const std::optional<std::size_t> copied_size = copied_map.size();
  REQUIRE(copied_size.has_value());
  CHECK(*copied_size == 2);
}
