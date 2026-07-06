// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - deserializes list fields from arrays", "[serialization]") {
  require_registered_type<caors::list<int>>("caors.list<int>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<list_actor>("caors.test.serialization.list_actor")
          .field("scores", &list_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<list_actor>()));

  caors::variant::array_type scores;
  scores.push_back(caors::variant::from(std::int64_t{5}));
  scores.push_back(caors::variant::from(std::int64_t{6}));

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"scores"},
      0,
      caors::variant::array(std::move(scores)),
  });

  list_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  REQUIRE(actor.scores.size() == 2);
  auto iterator = actor.scores.begin();
  CHECK(*iterator == 5);
  ++iterator;
  CHECK(*iterator == 6);
}
