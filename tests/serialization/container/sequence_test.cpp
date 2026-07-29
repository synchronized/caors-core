// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_sequence_deserializes_list_fields_from_arrays_test.cpp

#include "serialization_test_support.h"

#include "caors/memory/stack.h"
#include "caors/memory/tracking.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - deserializes list fields from arrays", "[serialization]") {
  require_serializable_property_type<list_actor>("caors.test.serialization.list_actor", "scores",
                                                 &list_actor::scores);

  caors::variant::array_type scores;
  scores.push_back(caors::variant::from(std::int64_t{5}));
  scores.push_back(caors::variant::from(std::int64_t{6}));

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"scores"},
      0,
      caors::variant::array(std::move(scores)),
  });

  list_actor actor{};
  const caors::error result =
      caors::read_value_from(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  REQUIRE(actor.scores.size() == 2);
  auto iterator = actor.scores.begin();
  CHECK(*iterator == 5);
  ++iterator;
  CHECK(*iterator == 6);
}

// 原测试文件：serialization_sequence_generic_sequence_traits_work_without_type_registration_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - generic sequence traits work without type registration",
          "[serialization]") {
  require_serializable_property_type<custom_sequence_actor>(
      "caors.test.serialization.custom_sequence_actor", "scores", &custom_sequence_actor::scores);

  custom_sequence_actor actor{};
  actor.scores.values.push_back(4);
  actor.scores.values.push_back(5);

  const caors::serialization::serialization_text_result text = caors::serialization::to_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[4,5]})");

  caors::variant::array_type values;
  values.push_back(caors::variant::from(std::int64_t{8}));
  values.push_back(caors::variant::from(std::int64_t{9}));

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"scores"},
      0,
      caors::variant::array(std::move(values)),
  });

  custom_sequence_actor decoded{};
  const caors::error result =
      caors::read_value_from(decoded, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  REQUIRE(decoded.scores.values.size() == 2);
  CHECK(decoded.scores.values[0] == 8);
  CHECK(decoded.scores.values[1] == 9);
}

// 原测试文件：serialization_sequence_rejects_std_array_length_mismatch_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects std array length mismatch", "[serialization]") {
  require_serializable_property_type<array_actor>("caors.test.serialization.array_actor", "scores",
                                                  &array_actor::scores);

  array_actor actor{{1, 2, 3}};
  const caors::error result = caors::read_json_from(actor, R"({"scores":[5,6]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_argument);
  CHECK(actor.scores == std::array<int, 3>{1, 2, 3});
}

// 原测试文件：serialization_sequence_rejects_std_span_deserialization_by_default_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects std span deserialization by default", "[serialization]") {
  require_serializable_property_type<span_actor>("caors.test.serialization.span_actor", "scores",
                                                 &span_actor::scores);

  std::array<int, 3> storage{1, 2, 3};
  span_actor actor{std::span<int>{storage}};

  const caors::error result = caors::read_json_from(actor, R"({"scores":[5,6,7]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
  CHECK(storage == std::array<int, 3>{1, 2, 3});
}

// 原测试文件：serialization_sequence_rejects_std_span_length_mismatch_without_writing_storage_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects std span length mismatch without writing storage",
          "[serialization]") {
  require_serializable_property_type<span_actor>("caors.test.serialization.span_actor", "scores",
                                                 &span_actor::scores);

  std::array<int, 3> storage{1, 2, 3};
  span_actor actor{std::span<int>{storage}};
  const caors::error result = caors::read_json_from(actor, R"({"scores":[5,6]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
  CHECK(storage == std::array<int, 3>{1, 2, 3});
}

TEST_CASE("serialization - writes std span caller buffers only when explicitly enabled",
          "[serialization]") {
  require_serializable_property_type<span_actor>("caors.test.serialization.span_actor", "scores",
                                                 &span_actor::scores);

  std::array<int, 3> storage{1, 2, 3};
  span_actor actor{std::span<int>{storage}};
  caors::serialization::context context;
  context.config.borrowed_views.allow_caller_buffer_write = true;

  REQUIRE(context.satisfies(caors::serialization::decode_requirement::caller_buffer));
  REQUIRE(caors::read_json_from(context, actor, R"({"scores":[5,6,7]})").succeeded());
  CHECK(storage == std::array<int, 3>{5, 6, 7});
}

TEST_CASE("serialization - releases std span transaction storage after decoding",
          "[serialization][memory]") {
  require_serializable_property_type<span_actor>("caors.test.serialization.span_actor", "scores",
                                                 &span_actor::scores);

  caors::tracking_resource temporary_tracking{&caors::get_system_memory_resource()};
  caors::stack_resource temporary{64, &temporary_tracking};
  std::array<int, 3> storage{1, 2, 3};
  span_actor actor{std::span<int>{storage}};
  caors::serialization::context context;
  context.config.borrowed_views.allow_caller_buffer_write = true;
  context.resolvers.memory.temporary_resource = &temporary;

  REQUIRE(caors::read_json_from(context, actor, R"({"scores":[5,6,7]})").succeeded());
  CHECK(storage == std::array<int, 3>{5, 6, 7});
  CHECK(temporary.active_scope_count() == 0);
  CHECK(temporary.statistics().mark_count > 0);
  CHECK(temporary.statistics().mark_count == temporary.statistics().rewind_count);

  temporary.reset();
  CHECK(temporary_tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("serialization - validates std span caller buffer length before writing",
          "[serialization]") {
  require_serializable_property_type<span_actor>("caors.test.serialization.span_actor", "scores",
                                                 &span_actor::scores);

  std::array<int, 3> storage{1, 2, 3};
  span_actor actor{std::span<int>{storage}};
  caors::serialization::context context;
  context.config.borrowed_views.allow_caller_buffer_write = true;

  const caors::error result = caors::read_json_from(context, actor, R"({"scores":[5,6]})");
  CHECK(result == caors::error{caors::error_domain::serialization, caors::errc::invalid_argument});
  CHECK(storage == std::array<int, 3>{1, 2, 3});
}

TEST_CASE("serialization - keeps std span caller buffers unchanged when an element fails",
          "[serialization]") {
  require_serializable_property_type<span_actor>("caors.test.serialization.span_actor", "scores",
                                                 &span_actor::scores);

  std::array<int, 3> storage{1, 2, 3};
  span_actor actor{std::span<int>{storage}};
  caors::serialization::context context;
  context.config.borrowed_views.allow_caller_buffer_write = true;

  const caors::error result =
      caors::read_json_from(context, actor, R"({"scores":[5,"invalid",7]})");
  CHECK(result.failed());
  CHECK(storage == std::array<int, 3>{1, 2, 3});
}

// 原测试文件：serialization_sequence_roundtrips_std_array_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips std array fields", "[serialization]") {
  require_serializable_property_type<array_actor>("caors.test.serialization.array_actor", "scores",
                                                  &array_actor::scores);

  array_actor actor{{1, 2, 3}};

  const caors::serialization::serialization_text_result text = caors::serialization::to_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[1,2,3]})");

  array_actor decoded{};
  const caors::error result = caors::read_json_from(decoded, R"({"scores":[5,6,7]})");

  REQUIRE(result.succeeded());
  CHECK(decoded.scores == std::array<int, 3>{5, 6, 7});
}

// 原测试文件：serialization_sequence_roundtrips_std_deque_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips std deque fields", "[serialization]") {
  require_serializable_property_type<deque_actor>("caors.test.serialization.deque_actor", "scores",
                                                  &deque_actor::scores);

  deque_actor actor{};
  actor.scores.push_back(1);
  actor.scores.push_back(2);
  actor.scores.push_back(3);

  const caors::serialization::serialization_text_result text = caors::serialization::to_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[1,2,3]})");

  deque_actor decoded{};
  const caors::error result = caors::read_json_from(decoded, R"({"scores":[5,6]})");

  REQUIRE(result.succeeded());
  REQUIRE(decoded.scores.size() == 2);
  CHECK(decoded.scores[0] == 5);
  CHECK(decoded.scores[1] == 6);
}

// 原测试文件：serialization_sequence_roundtrips_std_forward_list_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips std forward list fields", "[serialization]") {
  require_serializable_property_type<forward_list_actor>(
      "caors.test.serialization.forward_list_actor", "scores", &forward_list_actor::scores);

  forward_list_actor actor{};
  actor.scores.assign({1, 2, 3});

  const caors::serialization::serialization_text_result text = caors::serialization::to_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[1,2,3]})");

  forward_list_actor decoded{};
  const caors::error result = caors::read_json_from(decoded, R"({"scores":[5,6]})");

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

// 原测试文件：serialization_sequence_serializes_list_fields_as_arrays_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes list fields as arrays", "[serialization]") {
  require_serializable_property_type<list_actor>("caors.test.serialization.list_actor", "scores",
                                                 &list_actor::scores);

  list_actor actor{};
  actor.scores.push_back(1);
  actor.scores.push_back(2);
  actor.scores.push_back(3);

  const caors::serialization::serialization_text_result text = caors::serialization::to_text(actor);
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

// 原测试文件：serialization_sequence_serializes_std_span_fields_as_borrowed_views_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes std span fields as borrowed views", "[serialization]") {
  require_serializable_property_type<span_actor>("caors.test.serialization.span_actor", "scores",
                                                 &span_actor::scores);

  std::array<int, 3> storage{1, 2, 3};
  span_actor actor{std::span<int>{storage}};

  const caors::serialization::serialization_text_result text = caors::serialization::to_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[1,2,3]})");
}
