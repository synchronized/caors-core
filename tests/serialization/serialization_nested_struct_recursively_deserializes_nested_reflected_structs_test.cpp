// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - recursively deserializes nested reflected structs", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<nested_stats>("caors.test.serialization.nested_stats")
          .field("level", &nested_stats::level)
          .field("score", &nested_stats::score)
          .commit()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<nested_actor>("caors.test.serialization.nested_actor")
          .field("id", &nested_actor::id)
          .field("stats", &nested_actor::stats)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<nested_stats>()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<nested_actor>()));

  caors::variant::object_type stats_members;
  stats_members.push_back(caors::value::variant_member{
      caors::string_name{"level"},
      0,
      caors::variant::from(std::int64_t{4}),
  });
  stats_members.push_back(caors::value::variant_member{
      caors::string_name{"score"},
      0,
      caors::variant::from(15.25),
  });

  caors::variant::object_type actor_members;
  actor_members.push_back(caors::value::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{9}),
  });
  actor_members.push_back(caors::value::variant_member{
      caors::string_name{"stats"},
      0,
      caors::variant::object(std::move(stats_members)),
  });

  nested_actor actor{};
  const caors::error result =
      caors::deserialize(actor, caors::variant::object(std::move(actor_members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id == 9);
  CHECK(actor.stats.level == 4);
  CHECK(actor.stats.score == Catch::Approx(15.25));
}
