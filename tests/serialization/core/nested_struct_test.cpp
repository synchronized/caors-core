// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_nested_struct_json_handles_nested_structs_arrays_and_schema_context_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - json handles nested structs arrays and schema context",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::type_<nested_stats>("caors.test.serialization.nested_stats"),
       caors::reflection::class_<nested_stats>())
          .field("level", &nested_stats::level)
          .field("score", &nested_stats::score)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<nested_stats>().commit()));
  REQUIRE(reflection_result_ok(((void)caors::meta::type_<nested_vector_actor>(
                                    "caors.test.serialization.nested_vector_actor"),
                                caors::reflection::class_<nested_vector_actor>())
                                   .field("stats", &nested_vector_actor::stats)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<nested_vector_actor>().commit()));

  nested_vector_actor actor{};
  actor.stats.push_back(nested_stats{1, 2.5F});
  actor.stats.push_back(nested_stats{3, 4.5F});

  const caors::serialization::serialization_text_result json = caors::serialize_json(actor);
  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"stats":[{"level":1,"score":2.5},{"level":3,"score":4.5}]})");
}

// 原测试文件：serialization_nested_struct_recursively_deserializes_nested_reflected_structs_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - recursively deserializes nested reflected structs", "[serialization]") {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::type_<nested_stats>("caors.test.serialization.nested_stats"),
       caors::reflection::class_<nested_stats>())
          .field("level", &nested_stats::level)
          .field("score", &nested_stats::score)
          .commit()));
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::type_<nested_actor>("caors.test.serialization.nested_actor"),
       caors::reflection::class_<nested_actor>())
          .field("id", &nested_actor::id)
          .field("stats", &nested_actor::stats)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<nested_stats>().commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<nested_actor>().commit()));

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

// 原测试文件：serialization_nested_struct_recursively_serializes_nested_reflected_structs_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - recursively serializes nested reflected structs", "[serialization]") {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::type_<nested_stats>("caors.test.serialization.nested_stats"),
       caors::reflection::class_<nested_stats>())
          .field("level", &nested_stats::level)
          .field("score", &nested_stats::score)
          .commit()));
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::type_<nested_actor>("caors.test.serialization.nested_actor"),
       caors::reflection::class_<nested_actor>())
          .field("id", &nested_actor::id)
          .field("stats", &nested_actor::stats)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<nested_stats>().commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<nested_actor>().commit()));

  const nested_actor actor{7, nested_stats{3, 12.5F}};
  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"id":7,"stats":{"level":3,"score":12.5}})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());
  const caors::value::variant_member* stats = value.value.find_member(caors::string_name{"stats"});
  REQUIRE(stats != nullptr);
  REQUIRE(stats->value.is_object());
  const caors::value::variant_member* level = stats->value.find_member(caors::string_name{"level"});
  REQUIRE(level != nullptr);
  CHECK(level->value.as_i64() == 3);
}
