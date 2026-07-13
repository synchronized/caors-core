// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_pointer_wrapper_deserializes_non_null_values_into_empty_wrappers_test.cpp

#include "serialization_test_support.h"

#include <memory>

using namespace caors::tests::serialization;

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

// 原测试文件：serialization_pointer_wrapper_deserializes_null_pointer_and_wrapper_fields_test.cpp

#include "serialization_test_support.h"

#include <memory>

using namespace caors::tests::serialization;

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

// 原测试文件：serialization_pointer_wrapper_deserializes_pointer_and_wrapper_fields_into_existing_values_test.cpp

#include "serialization_test_support.h"

#include <memory>

using namespace caors::tests::serialization;

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

// 原测试文件：serialization_pointer_wrapper_rejects_non_null_values_for_empty_raw_pointers_test.cpp

#include "serialization_test_support.h"

#include <memory>

using namespace caors::tests::serialization;

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

// 原测试文件：serialization_pointer_wrapper_rejects_null_reference_wrapper_fields_test.cpp

#include "serialization_test_support.h"

#include <memory>

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects null reference wrapper fields", "[serialization]") {
  require_reference_wrapper_actor_metadata();

  reference_wrapper_actor actor{7};
  const caors::error result = caors::deserialize_json(actor, R"({"value":null})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_field);
  CHECK(actor.storage == 7);
}

// 原测试文件：serialization_pointer_wrapper_serializes_and_deserializes_reference_wrapper_fields_test.cpp

#include "serialization_test_support.h"

#include <memory>

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes and deserializes reference wrapper fields",
          "[serialization]") {
  require_reference_wrapper_actor_metadata();

  reference_wrapper_actor actor{7};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"value":7})");

  const caors::error result = caors::deserialize_json(actor, R"({"value":11})");

  REQUIRE(result.succeeded());
  CHECK(&actor.value.get() == &actor.storage);
  CHECK(actor.storage == 11);
}

// 原测试文件：serialization_pointer_wrapper_serializes_empty_pointer_and_wrapper_fields_as_null_test.cpp

#include "serialization_test_support.h"

#include <memory>

using namespace caors::tests::serialization;

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

// 原测试文件：serialization_pointer_wrapper_serializes_pointer_and_wrapper_fields_as_inline_values_test.cpp

#include "serialization_test_support.h"

#include <memory>

using namespace caors::tests::serialization;

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

TEST_CASE("serialization - stops raw pointer inline recursion at configured depth",
          "[serialization]") {
  require_recursive_node_metadata();

  recursive_node node{1, nullptr};
  node.next = &node;

  caors::serialization::context context{};
  context.limits.max_depth = 2;

  const caors::serialization::serialization_text_result text = caors::serialize_text(context, node);

  REQUIRE(text.failed());
  CHECK(text.status.domain() == caors::error_domain::serialization);
  CHECK(text.status.value() == caors::errc::recursion_limit);
}

TEST_CASE("serialization - stops raw pointer deserialization recursion at configured depth",
          "[serialization]") {
  require_recursive_node_metadata();

  recursive_node root{};
  recursive_node child{};
  recursive_node grandchild{};
  root.next = &child;
  child.next = &grandchild;

  caors::serialization::context context{};
  context.limits.max_depth = 2;

  const caors::error result = caors::deserialize_json(
      context, root, R"({"value":1,"next":{"value":2,"next":{"value":3,"next":null}}})");

  REQUIRE(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::recursion_limit);
}

TEST_CASE("serialization - can disable inline recursion depth limit", "[serialization]") {
  require_recursive_node_metadata();

  recursive_node child{2, nullptr};
  recursive_node root{1, &child};

  caors::serialization::context context{};
  context.limits.max_depth = 0;

  const caors::serialization::serialization_text_result text = caors::serialize_text(context, root);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"value":1,"next":{"value":2,"next":null}})");
}
