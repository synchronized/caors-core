// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes list fields as arrays", "[serialization]") {
  require_registered_type<caors::list<int>>("caors.list<int>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<list_actor>("caors.test.serialization.list_actor")
          .field("scores", &list_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<list_actor>()));

  list_actor actor{};
  actor.scores.push_back(1);
  actor.scores.push_back(2);
  actor.scores.push_back(3);

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[1,2,3]})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* scores =
      value.value.find_member(caors::string_name{"scores"});
  REQUIRE(scores != nullptr);
  REQUIRE(scores->value.is_array());
  REQUIRE(scores->value.as_array().size() == 3);
  CHECK(scores->value.as_array()[2].as_i64() == 3);
}

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

TEST_CASE("serialization - roundtrips std forward list fields", "[serialization]") {
  using scores_type = std::forward_list<int>;
  require_registered_type<scores_type>("std.forward_list<int>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<forward_list_actor>("caors.test.serialization.forward_list_actor")
          .field("scores", &forward_list_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<forward_list_actor>()));

  forward_list_actor actor{};
  actor.scores.assign({1, 2, 3});

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[1,2,3]})");

  forward_list_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"scores":[5,6]})");

  REQUIRE(result.succeeded());
  auto iterator = decoded.scores.begin();
  REQUIRE(iterator != decoded.scores.end());
  CHECK(*iterator == 5);
  ++iterator;
  REQUIRE(iterator != decoded.scores.end());
  CHECK(*iterator == 6);
  ++iterator;
  CHECK(iterator == decoded.scores.end());
}

TEST_CASE("serialization - roundtrips std deque fields", "[serialization]") {
  using scores_type = std::deque<int>;
  require_registered_type<scores_type>("std.deque<int>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<deque_actor>("caors.test.serialization.deque_actor")
          .field("scores", &deque_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<deque_actor>()));

  deque_actor actor{};
  actor.scores.push_back(1);
  actor.scores.push_back(2);
  actor.scores.push_back(3);

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[1,2,3]})");

  deque_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"scores":[5,6]})");

  REQUIRE(result.succeeded());
  REQUIRE(decoded.scores.size() == 2);
  CHECK(decoded.scores[0] == 5);
  CHECK(decoded.scores[1] == 6);
}

TEST_CASE("serialization - roundtrips std array fields", "[serialization]") {
  using scores_type = std::array<int, 3>;
  require_registered_type<scores_type>("std.array<int,3>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<array_actor>("caors.test.serialization.array_actor")
          .field("scores", &array_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<array_actor>()));

  array_actor actor{{1, 2, 3}};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[1,2,3]})");

  array_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"scores":[5,6,7]})");

  REQUIRE(result.succeeded());
  CHECK(decoded.scores == std::array<int, 3>{5, 6, 7});
}

TEST_CASE("serialization - rejects std array length mismatch", "[serialization]") {
  using scores_type = std::array<int, 3>;
  require_registered_type<scores_type>("std.array<int,3>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<array_actor>("caors.test.serialization.array_actor")
          .field("scores", &array_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<array_actor>()));

  array_actor actor{{1, 2, 3}};
  const caors::error result = caors::deserialize_json(actor, R"({"scores":[5,6]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_argument);
  CHECK(actor.scores == std::array<int, 3>{1, 2, 3});
}

TEST_CASE("serialization - serializes std span fields as borrowed views", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<span_actor>("caors.test.serialization.span_actor")
          .field("scores", &span_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<span_actor>()));

  std::array<int, 3> storage{1, 2, 3};
  span_actor actor{std::span<int>{storage}};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[1,2,3]})");
}

TEST_CASE("serialization - rejects std span deserialization by default", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<span_actor>("caors.test.serialization.span_actor")
          .field("scores", &span_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<span_actor>()));

  std::array<int, 3> storage{1, 2, 3};
  span_actor actor{std::span<int>{storage}};

  const caors::error result = caors::deserialize_json(actor, R"({"scores":[5,6,7]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
  CHECK(storage == std::array<int, 3>{1, 2, 3});
}

TEST_CASE("serialization - rejects std span length mismatch without writing storage",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<span_actor>("caors.test.serialization.span_actor")
          .field("scores", &span_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<span_actor>()));

  std::array<int, 3> storage{1, 2, 3};
  span_actor actor{std::span<int>{storage}};
  const caors::error result = caors::deserialize_json(actor, R"({"scores":[5,6]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
  CHECK(storage == std::array<int, 3>{1, 2, 3});
}

TEST_CASE("serialization - generic sequence traits work without codec registration",
          "[serialization]") {
  using scores_type = custom_codec_sequence<int>;
  require_registered_type<scores_type>("caors.test.serialization.custom_sequence<int>");
  REQUIRE(reflection_result_ok(caors::reflection::class_<custom_codec_sequence_actor>(
                                   "caors.test.serialization.custom_codec_sequence_actor")
                                   .field("scores", &custom_codec_sequence_actor::scores)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::register_type<custom_codec_sequence_actor>()));

  custom_codec_sequence_actor actor{};
  actor.scores.values.push_back(4);
  actor.scores.values.push_back(5);

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[4,5]})");

  caors::variant::array_type values;
  values.push_back(caors::variant::from(std::int64_t{8}));
  values.push_back(caors::variant::from(std::int64_t{9}));

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"scores"},
      0,
      caors::variant::array(std::move(values)),
  });

  custom_codec_sequence_actor decoded{};
  const caors::error result =
      caors::deserialize(decoded, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  REQUIRE(decoded.scores.values.size() == 2);
  CHECK(decoded.scores.values[0] == 8);
  CHECK(decoded.scores.values[1] == 9);
}
