// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - recursively serializes vector struct elements", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<nested_stats>("caors.test.serialization.nested_stats")
          .field("level", &nested_stats::level)
          .field("score", &nested_stats::score)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<nested_stats>()));
  require_registered_type<caors::vector<nested_stats>>(
      "caors.vector<caors.test.serialization.nested_stats>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<nested_vector_actor>("caors.test.serialization.nested_vector_actor")
          .field("stats", &nested_vector_actor::stats)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<nested_vector_actor>()));

  nested_vector_actor actor{};
  actor.stats.push_back(nested_stats{1, 2.5F});
  actor.stats.push_back(nested_stats{3, 4.5F});

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"stats":[{"level":1,"score":2.5},{"level":3,"score":4.5}]})");

  caors::variant::object_type first_stats;
  first_stats.push_back(caors::value::variant_member{
      caors::string_name{"level"},
      0,
      caors::variant::from(std::int64_t{7}),
  });
  first_stats.push_back(caors::value::variant_member{
      caors::string_name{"score"},
      0,
      caors::variant::from(8.5),
  });

  caors::variant::array_type stats_array;
  stats_array.push_back(caors::variant::object(std::move(first_stats)));

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"stats"},
      0,
      caors::variant::array(std::move(stats_array)),
  });

  nested_vector_actor decoded{};
  const caors::error result =
      caors::deserialize(decoded, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  REQUIRE(decoded.stats.size() == 1);
  CHECK(decoded.stats[0].level == 7);
  CHECK(decoded.stats[0].score == Catch::Approx(8.5));
}
