// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - recursively serializes nested reflected structs", "[serialization]") {
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

TEST_CASE("serialization - json handles nested structs arrays and schema context",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<nested_stats>("caors.test.serialization.nested_stats")
          .field("level", &nested_stats::level)
          .field("score", &nested_stats::score)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<nested_stats>()));
  require_serialization_codec<caors::vector<nested_stats>>(
      "caors.vector<caors.test.serialization.nested_stats>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<nested_vector_actor>("caors.test.serialization.nested_vector_actor")
          .field("stats", &nested_vector_actor::stats)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<nested_vector_actor>()));

  nested_vector_actor actor{};
  actor.stats.push_back(nested_stats{1, 2.5F});
  actor.stats.push_back(nested_stats{3, 4.5F});

  const caors::serialization::serialization_text_result json = caors::serialize_json(actor);
  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"stats":[{"level":1,"score":2.5},{"level":3,"score":4.5}]})");
}

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

TEST_CASE("serialization - updates non default constructible direct fields", "[serialization]") {
  require_non_default_value_metadata();

  non_default_field_actor actor{non_default_value{1}};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"value":{"value":1}})");

  const caors::error result = caors::deserialize_json(actor, R"({"value":{"value":7}})");

  REQUIRE(result.succeeded());
  CHECK(actor.value.value == 7);
}

TEST_CASE("serialization - rejects non default constructible sequence elements",
          "[serialization]") {
  require_non_default_value_metadata();

  non_default_vector_actor actor{};
  actor.values.emplace_back(1);

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"values":[{"value":1}]})");

  const caors::error result = caors::deserialize_json(actor, R"({"values":[{"value":7}]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
  REQUIRE(actor.values.size() == 1);
  CHECK(actor.values[0].value == 1);
}

TEST_CASE("serialization - updates existing non default constructible unique pointers",
          "[serialization]") {
  require_non_default_value_metadata();

  non_default_unique_ptr_actor actor{};
  actor.value = std::make_unique<non_default_value>(1);

  const caors::error result = caors::deserialize_json(actor, R"({"value":{"value":7}})");

  REQUIRE(result.succeeded());
  REQUIRE(actor.value != nullptr);
  CHECK(actor.value->value == 7);
}

TEST_CASE("serialization - rejects empty non default constructible unique pointers",
          "[serialization]") {
  require_non_default_value_metadata();

  non_default_unique_ptr_actor actor{};

  const caors::error result = caors::deserialize_json(actor, R"({"value":{"value":7}})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_field);
  CHECK(actor.value == nullptr);
}

TEST_CASE("serialization - updates existing non default constructible variant alternatives",
          "[serialization]") {
  require_non_default_value_metadata();

  non_default_variant_actor actor{};
  actor.value.emplace<1>(1);

  const caors::error result = caors::deserialize_json(actor, R"({"value":[1,{"value":7}]})");

  REQUIRE(result.succeeded());
  REQUIRE(std::holds_alternative<non_default_value>(actor.value));
  CHECK(std::get<non_default_value>(actor.value).value == 7);
}

TEST_CASE("serialization - rejects switching to non default constructible variant alternatives",
          "[serialization]") {
  require_non_default_value_metadata();

  non_default_variant_actor actor{};
  actor.value = 3;

  const caors::error result = caors::deserialize_json(actor, R"({"value":[1,{"value":7}]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
  REQUIRE(std::holds_alternative<int>(actor.value));
  CHECK(std::get<int>(actor.value) == 3);
}

TEST_CASE("serialization - serializes pointer and wrapper fields as inline values",
          "[serialization]") {
  require_pointer_actor_metadata();

  int raw = 7;
  nested_stats stats{2, 3.5F};
  pointer_actor actor{};
  actor.raw = &raw;
  actor.shared = std::make_shared<int>(8);
  actor.unique = std::make_unique<int>(9);
  actor.optional = 10;
  actor.stats = &stats;

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value ==
        R"({"raw":7,"shared":8,"unique":9,"optional":10,"stats":{"level":2,"score":3.5}})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* shared =
      value.value.find_member(caors::string_name{"shared"});
  REQUIRE(shared != nullptr);
  CHECK(shared->value.as_i64() == 8);

  const caors::value::variant_member* nested = value.value.find_member(caors::string_name{"stats"});
  REQUIRE(nested != nullptr);
  REQUIRE(nested->value.is_object());
  const caors::value::variant_member* level =
      nested->value.find_member(caors::string_name{"level"});
  REQUIRE(level != nullptr);
  CHECK(level->value.as_i64() == 2);
}

TEST_CASE("serialization - deserializes pointer and wrapper fields into existing values",
          "[serialization]") {
  require_pointer_actor_metadata();

  caors::variant::object_type stats_members;
  stats_members.push_back(caors::value::variant_member{
      caors::string_name{"level"},
      0,
      caors::variant::from(std::int64_t{4}),
  });
  stats_members.push_back(caors::value::variant_member{
      caors::string_name{"score"},
      0,
      caors::variant::from(5.5),
  });

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"raw"},
      0,
      caors::variant::from(std::int64_t{70}),
  });
  members.push_back(caors::value::variant_member{
      caors::string_name{"shared"},
      0,
      caors::variant::from(std::int64_t{80}),
  });
  members.push_back(caors::value::variant_member{
      caors::string_name{"unique"},
      0,
      caors::variant::from(std::int64_t{90}),
  });
  members.push_back(caors::value::variant_member{
      caors::string_name{"optional"},
      0,
      caors::variant::from(std::int64_t{100}),
  });
  members.push_back(caors::value::variant_member{
      caors::string_name{"stats"},
      0,
      caors::variant::object(std::move(stats_members)),
  });

  int raw = 0;
  nested_stats stats{};
  pointer_actor actor{};
  actor.raw = &raw;
  actor.shared = std::make_shared<int>(0);
  actor.unique = std::make_unique<int>(0);
  actor.optional = 0;
  actor.stats = &stats;

  const int* raw_address = actor.raw;
  const int* shared_address = actor.shared.get();
  const int* unique_address = actor.unique.get();
  nested_stats* stats_address = actor.stats;

  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.raw == raw_address);
  CHECK(actor.shared.get() == shared_address);
  CHECK(actor.unique.get() == unique_address);
  CHECK(actor.stats == stats_address);
  CHECK(raw == 70);
  CHECK(*actor.shared == 80);
  CHECK(*actor.unique == 90);
  REQUIRE(actor.optional.has_value());
  CHECK(*actor.optional == 100);
  CHECK(stats.level == 4);
  CHECK(stats.score == Catch::Approx(5.5));
}

TEST_CASE("serialization - serializes and deserializes reference wrapper fields",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<reference_wrapper_actor>(
          "caors.test.serialization.reference_wrapper_actor")
          .field("value", &reference_wrapper_actor::value, "std.reference_wrapper<int>")
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<reference_wrapper_actor>()));

  reference_wrapper_actor actor{7};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"value":7})");

  const caors::error result = caors::deserialize_json(actor, R"({"value":11})");

  REQUIRE(result.succeeded());
  CHECK(&actor.value.get() == &actor.storage);
  CHECK(actor.storage == 11);
}

TEST_CASE("serialization - rejects null reference wrapper fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<reference_wrapper_actor>(
          "caors.test.serialization.reference_wrapper_actor")
          .field("value", &reference_wrapper_actor::value, "std.reference_wrapper<int>")
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<reference_wrapper_actor>()));

  reference_wrapper_actor actor{7};
  const caors::error result = caors::deserialize_json(actor, R"({"value":null})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_field);
  CHECK(actor.storage == 7);
}

TEST_CASE("serialization - serializes empty pointer and wrapper fields as null",
          "[serialization]") {
  require_pointer_actor_metadata();

  pointer_actor actor{};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"raw":null,"shared":null,"unique":null,"optional":null,"stats":null})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());

  const caors::value::variant_member* raw = value.value.find_member(caors::string_name{"raw"});
  const caors::value::variant_member* shared =
      value.value.find_member(caors::string_name{"shared"});
  const caors::value::variant_member* unique =
      value.value.find_member(caors::string_name{"unique"});
  const caors::value::variant_member* optional =
      value.value.find_member(caors::string_name{"optional"});
  const caors::value::variant_member* stats = value.value.find_member(caors::string_name{"stats"});
  REQUIRE(raw != nullptr);
  REQUIRE(shared != nullptr);
  REQUIRE(unique != nullptr);
  REQUIRE(optional != nullptr);
  REQUIRE(stats != nullptr);
  CHECK(raw->value.is_null());
  CHECK(shared->value.is_null());
  CHECK(unique->value.is_null());
  CHECK(optional->value.is_null());
  CHECK(stats->value.is_null());
}

TEST_CASE("serialization - deserializes null pointer and wrapper fields", "[serialization]") {
  require_pointer_actor_metadata();

  int raw = 1;
  nested_stats stats{1, 2.0F};
  pointer_actor actor{};
  actor.raw = &raw;
  actor.shared = std::make_shared<int>(8);
  actor.unique = std::make_unique<int>(9);
  actor.optional = 10;
  actor.stats = &stats;

  const caors::error result = caors::deserialize_json(
      actor, R"({"raw":null,"shared":null,"unique":null,"optional":null,"stats":null})");

  REQUIRE(result.succeeded());
  CHECK(actor.raw == nullptr);
  CHECK(actor.shared == nullptr);
  CHECK(actor.unique == nullptr);
  CHECK_FALSE(actor.optional.has_value());
  CHECK(actor.stats == nullptr);
}

TEST_CASE("serialization - deserializes non-null values into empty wrappers", "[serialization]") {
  require_pointer_actor_metadata();

  int raw = 0;
  nested_stats stats{};
  pointer_actor actor{};
  actor.raw = &raw;
  actor.stats = &stats;

  const caors::error result = caors::deserialize_json(
      actor,
      R"({"raw":70,"shared":80,"unique":90,"optional":100,"stats":{"level":4,"score":5.5}})");

  REQUIRE(result.succeeded());
  CHECK(actor.raw == &raw);
  REQUIRE(actor.shared != nullptr);
  REQUIRE(actor.unique != nullptr);
  REQUIRE(actor.optional.has_value());
  CHECK(actor.stats == &stats);
  CHECK(raw == 70);
  CHECK(*actor.shared == 80);
  CHECK(*actor.unique == 90);
  CHECK(*actor.optional == 100);
  CHECK(stats.level == 4);
  CHECK(stats.score == Catch::Approx(5.5));
}

TEST_CASE("serialization - rejects non-null values for empty raw pointers", "[serialization]") {
  require_pointer_actor_metadata();

  pointer_actor actor{};

  const caors::error result = caors::deserialize_json(
      actor,
      R"({"raw":70,"shared":80,"unique":90,"optional":100,"stats":{"level":4,"score":5.5}})");

  REQUIRE(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_field);
  CHECK(actor.raw == nullptr);
  CHECK(actor.shared == nullptr);
  CHECK(actor.unique == nullptr);
  CHECK_FALSE(actor.optional.has_value());
  CHECK(actor.stats == nullptr);
}
