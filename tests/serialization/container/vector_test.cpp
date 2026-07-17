// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_vector_deserializes_vector_fields_from_arrays_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - deserializes vector fields from arrays", "[serialization]") {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::class_<vector_actor>("caors.test.serialization.vector_actor"),
       caors::meta::class_<vector_actor>())
          .property("scores", &vector_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<vector_actor>().commit()));

  caors::variant::array_type scores;
  scores.push_back(caors::variant::from(std::int64_t{5}));
  scores.push_back(caors::variant::from(std::int64_t{6}));

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"scores"},
      0,
      caors::variant::array(std::move(scores)),
  });

  vector_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  REQUIRE(actor.scores.size() == 2);
  CHECK(actor.scores[0] == 5);
  CHECK(actor.scores[1] == 6);
}

// 原测试文件：serialization_vector_recursively_serializes_codec_values_inside_vectors_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - recursively serializes codec values inside vectors", "[serialization]") {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::class_<guid_vector_actor>("caors.test.serialization.guid_vector_actor"),
       caors::meta::class_<guid_vector_actor>())
          .property("ids", &guid_vector_actor::ids)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<guid_vector_actor>().commit()));

  const std::optional<caors::guid> first =
      caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  const std::optional<caors::guid> second =
      caors::guid::parse("5ed39a97-7b20-4d87-86ac-e62a4a615b10");
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());

  guid_vector_actor actor{};
  actor.ids.push_back(*first);
  actor.ids.push_back(*second);

  const caors::serialization::serialization_text_result text =
      caors::serialization::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(
      text.value ==
      R"({"ids":["2f6d1f64-7a3a-4db2-9a2d-123456789abc","5ed39a97-7b20-4d87-86ac-e62a4a615b10"]})");

  guid_vector_actor decoded{};
  const caors::error result = caors::deserialize(
      decoded,
      R"({"ids":["5ed39a97-7b20-4d87-86ac-e62a4a615b10","2f6d1f64-7a3a-4db2-9a2d-123456789abc"]})");

  REQUIRE(result.succeeded());
  REQUIRE(decoded.ids.size() == 2);
  CHECK(decoded.ids[0] == *second);
  CHECK(decoded.ids[1] == *first);
}

// 原测试文件：serialization_vector_recursively_serializes_nested_vector_values_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - recursively serializes nested vector values", "[serialization]") {
  REQUIRE(reflection_result_ok(((void)caors::meta::class_<nested_vector_values_actor>(
                                    "caors.test.serialization.nested_vector_values_actor"),
                                caors::meta::class_<nested_vector_values_actor>())
                                   .property("groups", &nested_vector_values_actor::groups)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::class_<nested_vector_values_actor>().commit()));

  nested_vector_values_actor actor{};
  actor.groups.push_back(caors::vector<int>{1, 2});
  actor.groups.push_back(caors::vector<int>{3});

  const caors::serialization::serialization_text_result text =
      caors::serialization::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"groups":[[1,2],[3]]})");

  const caors::serialization::variant_result value = caors::to_value(actor);
  REQUIRE(value.succeeded());
  const caors::meta::variant_member* groups = value.value.find_member(caors::string_name{"groups"});
  REQUIRE(groups != nullptr);
  REQUIRE(groups->value.is_array());
  REQUIRE(groups->value.as_array().size() == 2);
  CHECK(groups->value.as_array()[0].as_array()[1].as_i64() == 2);

  nested_vector_values_actor decoded{};
  const caors::error result = caors::deserialize(decoded, R"({"groups":[[4,5],[6]]})");
  REQUIRE(result.succeeded());
  REQUIRE(decoded.groups.size() == 2);
  REQUIRE(decoded.groups[0].size() == 2);
  CHECK(decoded.groups[0][0] == 4);
  CHECK(decoded.groups[0][1] == 5);
  REQUIRE(decoded.groups[1].size() == 1);
  CHECK(decoded.groups[1][0] == 6);
}

// 原测试文件：serialization_vector_recursively_serializes_vector_struct_elements_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - recursively serializes vector struct elements", "[serialization]") {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::class_<nested_stats>("caors.test.serialization.nested_stats"),
       caors::meta::class_<nested_stats>())
          .property("level", &nested_stats::level)
          .property("score", &nested_stats::score)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<nested_stats>().commit()));
  REQUIRE(reflection_result_ok(((void)caors::meta::class_<nested_vector_actor>(
                                    "caors.test.serialization.nested_vector_actor"),
                                caors::meta::class_<nested_vector_actor>())
                                   .property("stats", &nested_vector_actor::stats)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<nested_vector_actor>().commit()));

  nested_vector_actor actor{};
  actor.stats.push_back(nested_stats{1, 2.5F});
  actor.stats.push_back(nested_stats{3, 4.5F});

  const caors::serialization::serialization_text_result text =
      caors::serialization::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"stats":[{"level":1,"score":2.5},{"level":3,"score":4.5}]})");

  caors::variant::object_type first_stats;
  first_stats.push_back(caors::meta::variant_member{
      caors::string_name{"level"},
      0,
      caors::variant::from(std::int64_t{7}),
  });
  first_stats.push_back(caors::meta::variant_member{
      caors::string_name{"score"},
      0,
      caors::variant::from(8.5),
  });

  caors::variant::array_type stats_array;
  stats_array.push_back(caors::variant::object(std::move(first_stats)));

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
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

// 原测试文件：serialization_vector_roundtrips_unique_pointer_vectors_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips unique pointer vectors", "[serialization]") {
  require_unique_ptr_vector_actor_metadata();

  unique_ptr_vector_actor actor{};
  actor.values.push_back(std::make_unique<int>(7));
  actor.values.push_back(std::unique_ptr<int>{});
  actor.values.push_back(std::make_unique<int>(9));

  const caors::serialization::serialization_text_result text = caors::to_string(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"values":[7,null,9]})");

  const caors::serialization::variant_result value = caors::to_value(actor);
  REQUIRE(value.succeeded());
  const caors::meta::variant_member* values = value.value.find_member(caors::string_name{"values"});
  REQUIRE(values != nullptr);
  REQUIRE(values->value.is_array());
  REQUIRE(values->value.as_array().size() == 3);
  CHECK(values->value.as_array()[0].as_i64() == 7);
  CHECK(values->value.as_array()[1].is_null());
  CHECK(values->value.as_array()[2].as_i64() == 9);

  unique_ptr_vector_actor decoded{};
  const caors::error result = caors::deserialize(decoded, R"({"values":[4,null,6]})");
  REQUIRE(result.succeeded());
  REQUIRE(decoded.values.size() == 3);
  REQUIRE(decoded.values[0] != nullptr);
  CHECK(*decoded.values[0] == 4);
  CHECK(decoded.values[1] == nullptr);
  REQUIRE(decoded.values[2] != nullptr);
  CHECK(*decoded.values[2] == 6);
}

// 原测试文件：serialization_vector_serializes_vector_fields_as_arrays_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes vector fields as arrays", "[serialization]") {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::class_<vector_actor>("caors.test.serialization.vector_actor"),
       caors::meta::class_<vector_actor>())
          .property("scores", &vector_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<vector_actor>().commit()));

  vector_actor actor{};
  actor.scores.push_back(1);
  actor.scores.push_back(2);
  actor.scores.push_back(3);

  const caors::serialization::serialization_text_result text =
      caors::serialization::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[1,2,3]})");

  const caors::serialization::variant_result value = caors::to_value(actor);
  REQUIRE(value.succeeded());
  const caors::meta::variant_member* scores = value.value.find_member(caors::string_name{"scores"});
  REQUIRE(scores != nullptr);
  REQUIRE(scores->value.is_array());
  REQUIRE(scores->value.as_array().size() == 3);
  CHECK(scores->value.as_array()[2].as_i64() == 3);
}
