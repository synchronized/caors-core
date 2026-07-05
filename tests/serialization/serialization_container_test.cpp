// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include "caors/memory.h"

using namespace caors::tests::serialization;

namespace {

struct resource_propagation_holder {
  using allocator_type = caors::allocator<resource_propagation_holder>;

  resource_propagation_holder() = default;

  explicit resource_propagation_holder(const allocator_type& allocator)
      : names(caors::allocator<caors::string>{allocator.resource()}),
        groups(caors::allocator<caors::vector<int>>{allocator.resource()}),
        labels(typename caors::hash_map<caors::string, caors::string>::allocator_type{
            allocator.resource()}) {}

  caors::vector<caors::string> names{};
  caors::vector<caors::vector<int>> groups{};
  caors::hash_map<caors::string, caors::string> labels{};
  std::optional<caors::string> maybe{};
  std::variant<int, caors::string, caors::vector<int>> choice{0};
};

void require_resource_propagation_holder_metadata() {
  require_serialization_codec<caors::vector<caors::string>>("caors.vector<caors.string>");
  require_serialization_codec<caors::vector<caors::vector<int>>>("caors.vector<caors.vector<int>>");
  require_serialization_codec<caors::hash_map<caors::string, caors::string>>(
      "caors.hash_map<caors.string,caors.string>");
  require_registered_type<std::optional<caors::string>>("std.optional<caors.string>");
  require_registered_type<std::variant<int, caors::string, caors::vector<int>>>(
      "std.variant<int,caors.string,caors.vector<int>>");

  REQUIRE(reflection_result_ok(
      caors::reflection::class_<resource_propagation_holder>(
          "caors.test.serialization.resource_propagation_holder")
          .field("names", &resource_propagation_holder::names, "caors.vector<caors.string>")
          .field("groups", &resource_propagation_holder::groups, "caors.vector<caors.vector<int>>")
          .field("labels", &resource_propagation_holder::labels,
                 "caors.hash_map<caors.string,caors.string>")
          .field("maybe", &resource_propagation_holder::maybe, "std.optional<caors.string>")
          .field("choice", &resource_propagation_holder::choice,
                 "std.variant<int,caors.string,caors.vector<int>>")
          .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::register_type<resource_propagation_holder>()));
}

} // namespace

TEST_CASE("serialization - propagates context resource through allocator aware values",
          "[serialization]") {
  require_resource_propagation_holder_metadata();

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::context context{};
  context.memory_resource = &tracking;

  struct holder_deleter {
    caors::memory_resource* resource = nullptr;

    void operator()(resource_propagation_holder* value) const noexcept {
      caors::destroy(value, resource);
    }
  };

  std::unique_ptr<resource_propagation_holder, holder_deleter> decoded{
      caors::make<resource_propagation_holder>(&tracking),
      holder_deleter{&tracking},
  };
  REQUIRE(decoded != nullptr);

  const caors::error result = caors::deserialize_json(
      *decoded,
      R"({"names":["Ada","Grace"],"groups":[[1,2],[3]],"labels":[["role","engineer"]],"maybe":"present","choice":[1,"variant"]})",
      context);

  REQUIRE(result.succeeded());

  CHECK(decoded->names.get_allocator().resource() == &tracking);
  REQUIRE(decoded->names.size() == 2);
  CHECK(decoded->names[0] == "Ada");
  CHECK(decoded->names[0].get_allocator().resource() == &tracking);
  CHECK(decoded->names[1].get_allocator().resource() == &tracking);

  CHECK(decoded->groups.get_allocator().resource() == &tracking);
  REQUIRE(decoded->groups.size() == 2);
  REQUIRE(decoded->groups[0].size() == 2);
  CHECK(decoded->groups[0].get_allocator().resource() == &tracking);
  CHECK(decoded->groups[0][0] == 1);
  CHECK(decoded->groups[0][1] == 2);
  CHECK(decoded->groups[1].get_allocator().resource() == &tracking);
  CHECK(decoded->groups[1][0] == 3);

  CHECK(decoded->labels.get_allocator().resource() == &tracking);
  {
    caors::string role_key{caors::allocator<char>{&tracking}};
    role_key = "role";
    const auto label = decoded->labels.find(role_key);
    REQUIRE(label != decoded->labels.end());
    CHECK(label->first.get_allocator().resource() == &tracking);
    CHECK(label->second == "engineer");
    CHECK(label->second.get_allocator().resource() == &tracking);
  }

  REQUIRE(decoded->maybe.has_value());
  CHECK(*decoded->maybe == "present");
  CHECK(decoded->maybe->get_allocator().resource() == &tracking);

  REQUIRE(std::holds_alternative<caors::string>(decoded->choice));
  CHECK(std::get<caors::string>(decoded->choice) == "variant");
  CHECK(std::get<caors::string>(decoded->choice).get_allocator().resource() == &tracking);

  decoded.reset();
  const caors::memory_statistics statistics = tracking.statistics();
  CHECK(statistics.bytes_in_use == 0);
  CHECK(statistics.allocation_count == statistics.deallocation_count);
}

TEST_CASE("serialization - serializes vector fields as arrays", "[serialization]") {
  require_serialization_codec<caors::vector<int>>("caors.vector<int>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<vector_actor>("caors.test.serialization.vector_actor")
          .field("scores", &vector_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<vector_actor>()));

  vector_actor actor{};
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

TEST_CASE("serialization - deserializes vector fields from arrays", "[serialization]") {
  require_serialization_codec<caors::vector<int>>("caors.vector<int>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<vector_actor>("caors.test.serialization.vector_actor")
          .field("scores", &vector_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<vector_actor>()));

  caors::variant::array_type scores;
  scores.push_back(caors::variant::from(std::int64_t{5}));
  scores.push_back(caors::variant::from(std::int64_t{6}));

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
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

TEST_CASE("serialization - recursively serializes nested vector values", "[serialization]") {
  require_serialization_codec<caors::vector<int>>("caors.vector<int>");
  require_registered_type<caors::vector<caors::vector<int>>>("caors.vector<caors.vector<int>>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<nested_vector_values_actor>(
          "caors.test.serialization.nested_vector_values_actor")
          .field("groups", &nested_vector_values_actor::groups, "caors.vector<caors.vector<int>>")
          .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::register_type<nested_vector_values_actor>()));

  nested_vector_values_actor actor{};
  actor.groups.push_back(caors::vector<int>{1, 2});
  actor.groups.push_back(caors::vector<int>{3});

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"groups":[[1,2],[3]]})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* groups =
      value.value.find_member(caors::string_name{"groups"});
  REQUIRE(groups != nullptr);
  REQUIRE(groups->value.is_array());
  REQUIRE(groups->value.as_array().size() == 2);
  CHECK(groups->value.as_array()[0].as_array()[1].as_i64() == 2);

  nested_vector_values_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"groups":[[4,5],[6]]})");
  REQUIRE(result.succeeded());
  REQUIRE(decoded.groups.size() == 2);
  REQUIRE(decoded.groups[0].size() == 2);
  CHECK(decoded.groups[0][0] == 4);
  CHECK(decoded.groups[0][1] == 5);
  REQUIRE(decoded.groups[1].size() == 1);
  CHECK(decoded.groups[1][0] == 6);
}

TEST_CASE("serialization - roundtrips unique pointer vectors", "[serialization]") {
  require_unique_ptr_vector_actor_metadata();

  unique_ptr_vector_actor actor{};
  actor.values.push_back(std::make_unique<int>(7));
  actor.values.push_back(std::unique_ptr<int>{});
  actor.values.push_back(std::make_unique<int>(9));

  const caors::serialization::serialization_text_result text = caors::serialize_json(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"values":[7,null,9]})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* values =
      value.value.find_member(caors::string_name{"values"});
  REQUIRE(values != nullptr);
  REQUIRE(values->value.is_array());
  REQUIRE(values->value.as_array().size() == 3);
  CHECK(values->value.as_array()[0].as_i64() == 7);
  CHECK(values->value.as_array()[1].is_null());
  CHECK(values->value.as_array()[2].as_i64() == 9);

  unique_ptr_vector_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"values":[4,null,6]})");
  REQUIRE(result.succeeded());
  REQUIRE(decoded.values.size() == 3);
  REQUIRE(decoded.values[0] != nullptr);
  CHECK(*decoded.values[0] == 4);
  CHECK(decoded.values[1] == nullptr);
  REQUIRE(decoded.values[2] != nullptr);
  CHECK(*decoded.values[2] == 6);
}

TEST_CASE("serialization - recursively serializes codec values inside vectors", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<guid_vector_actor>("caors.test.serialization.guid_vector_actor")
          .field("ids", &guid_vector_actor::ids, "caors.vector<guid>")
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<guid_vector_actor>()));

  const std::optional<caors::guid> first =
      caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  const std::optional<caors::guid> second =
      caors::guid::parse("5ed39a97-7b20-4d87-86ac-e62a4a615b10");
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());

  guid_vector_actor actor{};
  actor.ids.push_back(*first);
  actor.ids.push_back(*second);

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(
      text.value ==
      R"({"ids":["2f6d1f64-7a3a-4db2-9a2d-123456789abc","5ed39a97-7b20-4d87-86ac-e62a4a615b10"]})");

  guid_vector_actor decoded{};
  const caors::error result = caors::deserialize_json(
      decoded,
      R"({"ids":["5ed39a97-7b20-4d87-86ac-e62a4a615b10","2f6d1f64-7a3a-4db2-9a2d-123456789abc"]})");

  REQUIRE(result.succeeded());
  REQUIRE(decoded.ids.size() == 2);
  CHECK(decoded.ids[0] == *second);
  CHECK(decoded.ids[1] == *first);
}

TEST_CASE("serialization - serializes list fields as arrays", "[serialization]") {
  require_serialization_codec<caors::list<int>>("caors.list<int>");
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
  require_serialization_codec<caors::list<int>>("caors.list<int>");
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
  require_serialization_codec<scores_type>("std.forward_list<int>");
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
  require_serialization_codec<scores_type>("std.deque<int>");
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
  require_serialization_codec<scores_type>("std.array<int,3>");
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
  require_serialization_codec<scores_type>("std.array<int,3>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<array_actor>("caors.test.serialization.array_actor")
          .field("scores", &array_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<array_actor>()));

  array_actor actor{{1, 2, 3}};
  const caors::error result = caors::deserialize_json(actor, R"({"scores":[5,6]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
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

TEST_CASE("serialization - roundtrips std pair fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<pair_actor>("caors.test.serialization.pair_actor")
          .field("entry", &pair_actor::entry)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<pair_actor>()));

  pair_actor actor{{7, caors::string{"seven"}}};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"entry":[7,"seven"]})");

  pair_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"entry":[9,"nine"]})");

  REQUIRE(result.succeeded());
  CHECK(decoded.entry.first == 9);
  CHECK(decoded.entry.second == "nine");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* entry = value.value.find_member(caors::string_name{"entry"});
  REQUIRE(entry != nullptr);
  REQUIRE(entry->value.is_array());
  REQUIRE(entry->value.as_array().size() == 2);
  CHECK(entry->value.as_array()[0].as_i64() == 7);
  CHECK(entry->value.as_array()[1].as_string() == "seven");
}

TEST_CASE("serialization - rejects std pair length mismatch", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<pair_actor>("caors.test.serialization.pair_actor")
          .field("entry", &pair_actor::entry)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<pair_actor>()));

  pair_actor actor{{7, caors::string{"seven"}}};
  const caors::error result = caors::deserialize_json(actor, R"({"entry":[9]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
  CHECK(actor.entry.first == 7);
  CHECK(actor.entry.second == "seven");
}

TEST_CASE("serialization - roundtrips std tuple fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<tuple_actor>("caors.test.serialization.tuple_actor")
          .field("entry", &tuple_actor::entry)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<tuple_actor>()));

  tuple_actor actor{{7, caors::string{"seven"}, 1.5F}};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"entry":[7,"seven",1.5]})");

  tuple_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"entry":[9,"nine",2.5]})");

  REQUIRE(result.succeeded());
  CHECK(std::get<0>(decoded.entry) == 9);
  CHECK(std::get<1>(decoded.entry) == "nine");
  CHECK(std::get<2>(decoded.entry) == 2.5F);

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* entry = value.value.find_member(caors::string_name{"entry"});
  REQUIRE(entry != nullptr);
  REQUIRE(entry->value.is_array());
  REQUIRE(entry->value.as_array().size() == 3);
  CHECK(entry->value.as_array()[0].as_i64() == 7);
  CHECK(entry->value.as_array()[1].as_string() == "seven");
  CHECK(entry->value.as_array()[2].as_f64() == 1.5);
}

TEST_CASE("serialization - rejects std tuple length mismatch", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<tuple_actor>("caors.test.serialization.tuple_actor")
          .field("entry", &tuple_actor::entry)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<tuple_actor>()));

  tuple_actor actor{{7, caors::string{"seven"}, 1.5F}};
  const caors::error result = caors::deserialize_json(actor, R"({"entry":[9,"nine"]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
  CHECK(std::get<0>(actor.entry) == 7);
  CHECK(std::get<1>(actor.entry) == "seven");
  CHECK(std::get<2>(actor.entry) == 1.5F);
}

TEST_CASE("serialization - roundtrips std variant fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<variant_actor>("caors.test.serialization.variant_actor")
          .field("entry", &variant_actor::entry)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<variant_actor>()));

  variant_actor actor{};
  actor.entry = caors::string{"seven"};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"entry":[1,"seven"]})");

  variant_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"entry":[2,2.5]})");

  REQUIRE(result.succeeded());
  REQUIRE(std::holds_alternative<float>(decoded.entry));
  CHECK(std::get<float>(decoded.entry) == 2.5F);

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* entry = value.value.find_member(caors::string_name{"entry"});
  REQUIRE(entry != nullptr);
  REQUIRE(entry->value.is_array());
  REQUIRE(entry->value.as_array().size() == 2);
  CHECK(entry->value.as_array()[0].as_u64() == 1U);
  CHECK(entry->value.as_array()[1].as_string() == "seven");
}

TEST_CASE("serialization - rejects std variant invalid data", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<variant_actor>("caors.test.serialization.variant_actor")
          .field("entry", &variant_actor::entry)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<variant_actor>()));

  variant_actor actor{};
  actor.entry = caors::string{"seven"};
  const caors::error invalid_index = caors::deserialize_json(actor, R"({"entry":[9,"bad"]})");

  CHECK(invalid_index.failed());
  CHECK(invalid_index.domain() == caors::error_domain::serialization);
  CHECK(invalid_index.value() == caors::errc::unsupported);
  REQUIRE(std::holds_alternative<caors::string>(actor.entry));
  CHECK(std::get<caors::string>(actor.entry) == "seven");

  const caors::error invalid_length = caors::deserialize_json(actor, R"({"entry":[1]})");

  CHECK(invalid_length.failed());
  CHECK(invalid_length.domain() == caors::error_domain::serialization);
  CHECK(invalid_length.value() == caors::errc::unsupported);
  REQUIRE(std::holds_alternative<caors::string>(actor.entry));
  CHECK(std::get<caors::string>(actor.entry) == "seven");
}

TEST_CASE("serialization - serializes hash map fields as dictionaries", "[serialization]") {
  using labels_type = caors::hash_map<int, caors::string>;
  require_serialization_codec<labels_type>("caors.hash_map<int,caors.string>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<map_actor>("caors.test.serialization.map_actor")
          .field("labels", &map_actor::labels)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<map_actor>()));

  map_actor actor{};
  actor.labels.emplace(7, caors::string{"seven"});

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"labels":[[7,"seven"]]})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* labels =
      value.value.find_member(caors::string_name{"labels"});
  REQUIRE(labels != nullptr);
  REQUIRE(labels->value.is_dictionary());
  REQUIRE(labels->value.as_dictionary().size() == 1);
  CHECK(labels->value.as_dictionary()[0].key.as_i64() == 7);
  CHECK(labels->value.as_dictionary()[0].value.as_string() == "seven");
}

TEST_CASE("serialization - deserializes hash map fields from dictionaries", "[serialization]") {
  using labels_type = caors::hash_map<int, caors::string>;
  require_serialization_codec<labels_type>("caors.hash_map<int,caors.string>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<map_actor>("caors.test.serialization.map_actor")
          .field("labels", &map_actor::labels)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<map_actor>()));

  caors::variant::dictionary_type entries;
  entries.push_back(caors::value::variant_entry{
      caors::variant::from(std::int64_t{1}),
      caors::variant::from(std::string_view{"one"}),
  });
  entries.push_back(caors::value::variant_entry{
      caors::variant::from(std::int64_t{2}),
      caors::variant::from(std::string_view{"two"}),
  });

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"labels"},
      0,
      caors::variant::dictionary(std::move(entries)),
  });

  map_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  REQUIRE(actor.labels.size() == 2);
  CHECK(actor.labels.at(1) == "one");
  CHECK(actor.labels.at(2) == "two");
}

TEST_CASE("serialization - deserializes hash map fields from pair arrays", "[serialization]") {
  using labels_type = caors::hash_map<int, caors::string>;
  require_serialization_codec<labels_type>("caors.hash_map<int,caors.string>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<map_actor>("caors.test.serialization.map_actor")
          .field("labels", &map_actor::labels)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<map_actor>()));

  const caors::serialization::variant_result parsed =
      caors::parse_json(R"({"labels":[[3,"three"]]})");
  REQUIRE(parsed.succeeded());

  map_actor actor{};
  const caors::error result = caors::deserialize(actor, parsed.value);

  REQUIRE(result.succeeded());
  REQUIRE(actor.labels.size() == 1);
  CHECK(actor.labels.at(3) == "three");
}

TEST_CASE("serialization - roundtrips std map fields", "[serialization]") {
  using labels_type = std::map<int, caors::string>;
  require_serialization_codec<labels_type>("std.map<int,caors.string>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<ordered_map_actor>("caors.test.serialization.ordered_map_actor")
          .field("labels", &ordered_map_actor::labels)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<ordered_map_actor>()));

  ordered_map_actor actor{};
  actor.labels.emplace(2, caors::string{"two"});
  actor.labels.emplace(1, caors::string{"one"});

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"labels":[[1,"one"],[2,"two"]]})");

  ordered_map_actor decoded{};
  const caors::error result =
      caors::deserialize_json(decoded, R"({"labels":[[3,"three"],[4,"four"]]})");

  REQUIRE(result.succeeded());
  REQUIRE(decoded.labels.size() == 2);
  CHECK(decoded.labels.at(3) == "three");
  CHECK(decoded.labels.at(4) == "four");
}

TEST_CASE("serialization - serializes hash set fields as arrays", "[serialization]") {
  using ids_type = caors::hash_set<int>;
  require_serialization_codec<ids_type>("caors.hash_set<int>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<set_actor>("caors.test.serialization.set_actor")
          .field("ids", &set_actor::ids)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<set_actor>()));

  set_actor actor{};
  actor.ids.emplace(7);

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"ids":[7]})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* ids = value.value.find_member(caors::string_name{"ids"});
  REQUIRE(ids != nullptr);
  REQUIRE(ids->value.is_array());
  REQUIRE(ids->value.as_array().size() == 1);
  CHECK(ids->value.as_array()[0].as_i64() == 7);
}

TEST_CASE("serialization - deserializes hash set fields from arrays", "[serialization]") {
  using ids_type = caors::hash_set<int>;
  require_serialization_codec<ids_type>("caors.hash_set<int>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<set_actor>("caors.test.serialization.set_actor")
          .field("ids", &set_actor::ids)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<set_actor>()));

  caors::variant::array_type ids;
  ids.push_back(caors::variant::from(std::int64_t{1}));
  ids.push_back(caors::variant::from(std::int64_t{2}));

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"ids"},
      0,
      caors::variant::array(std::move(ids)),
  });

  set_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  REQUIRE(actor.ids.size() == 2);
  CHECK(actor.ids.contains(1));
  CHECK(actor.ids.contains(2));
}

TEST_CASE("serialization - hash set reader rejects duplicate elements", "[serialization]") {
  using ids_type = caors::hash_set<int>;
  require_serialization_codec<ids_type>("caors.hash_set<int>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<set_actor>("caors.test.serialization.set_actor")
          .field("ids", &set_actor::ids)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<set_actor>()));

  caors::variant::array_type ids;
  ids.push_back(caors::variant::from(std::int64_t{1}));
  ids.push_back(caors::variant::from(std::int64_t{1}));

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"ids"},
      0,
      caors::variant::array(std::move(ids)),
  });

  set_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::already_exists);
}

TEST_CASE("serialization - roundtrips std set fields", "[serialization]") {
  using ids_type = std::set<int>;
  require_serialization_codec<ids_type>("std.set<int>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<ordered_set_actor>("caors.test.serialization.ordered_set_actor")
          .field("ids", &ordered_set_actor::ids)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<ordered_set_actor>()));

  ordered_set_actor actor{};
  actor.ids.emplace(3);
  actor.ids.emplace(1);

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"ids":[1,3]})");

  ordered_set_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"ids":[2,4]})");

  REQUIRE(result.succeeded());
  REQUIRE(decoded.ids.size() == 2);
  CHECK(decoded.ids.contains(2));
  CHECK(decoded.ids.contains(4));
}

TEST_CASE("serialization - generic sequence codec uses sequence traits", "[serialization]") {
  using scores_type = custom_codec_sequence<int>;
  require_serialization_codec<scores_type>("caors.test.serialization.custom_sequence<int>");
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

TEST_CASE("serialization - generic associative codec uses associative traits", "[serialization]") {
  using labels_type = custom_codec_map<int, caors::string>;
  require_serialization_codec<labels_type>("caors.test.serialization.custom_map<int,caors.string>");
  REQUIRE(reflection_result_ok(caors::reflection::class_<custom_codec_map_actor>(
                                   "caors.test.serialization.custom_codec_map_actor")
                                   .field("labels", &custom_codec_map_actor::labels)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<custom_codec_map_actor>()));

  custom_codec_map_actor actor{};
  actor.labels.values.emplace(6, caors::string{"six"});

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"labels":[[6,"six"]]})");

  caors::variant::dictionary_type entries;
  entries.push_back(caors::value::variant_entry{
      caors::variant::from(std::int64_t{10}),
      caors::variant::from(std::string_view{"ten"}),
  });

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"labels"},
      0,
      caors::variant::dictionary(std::move(entries)),
  });

  custom_codec_map_actor decoded{};
  const caors::error result =
      caors::deserialize(decoded, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  REQUIRE(decoded.labels.values.size() == 1);
  CHECK(decoded.labels.values.at(10) == "ten");
}

TEST_CASE("serialization - recursively serializes vector struct elements", "[serialization]") {
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
