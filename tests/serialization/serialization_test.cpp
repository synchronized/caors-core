// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection/builder.h"
#include "caors/serialization.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {

struct serializable_actor {
  int id = 0;
  float health = 0.0F;
  bool active = false;
  caors::string name{};
};

struct configured_actor {
  int id = 0;
  float health = 0.0F;
  bool debug = false;
};

struct writer_actor {
  int id = 0;
};

struct dom_actor {
  int id = 0;
};

struct numeric_writer_actor {
  int id = 0;
};

struct reader_actor {
  int id = 0;
  float health = 0.0F;
  bool active = false;
  caors::string name{};
};

struct reader_field_id_actor {
  int id = 0;
};

struct reader_missing_field_actor {
  int id = 0;
};

struct reader_field_policy_actor {
  int id = 10;
  int optional = 20;
  int required = 30;
  int skipped = 40;
};

struct reader_unknown_field_actor {
  int id = 0;
};

struct versioned_actor {
  int id = 0;
  int legacy = 0;
  int current = 0;
  int future = 0;
};

struct codec_guid_actor {
  caors::guid id{};
  caors::type_guid type{};
};

struct codec_string_view_actor {
  std::string_view name{};
};

struct serialization_first_actor {
  int id = 0;
};

struct reflected_only_actor {
  int id = 0;
};

struct missing_field_actor {
  int id = 0;
};

struct unsupported_field_value {
  int id = 0;
};

struct unsupported_field_actor {
  unsupported_field_value value{};
};

struct nested_stats {
  int level = 0;
  float score = 0.0F;
};

struct nested_actor {
  int id = 0;
  nested_stats stats{};
};

struct vector_actor {
  caors::vector<int> scores{};
};

struct list_actor {
  caors::list<int> scores{};
};

struct map_actor {
  caors::hash_map<int, caors::string> labels{};
};

struct set_actor {
  caors::hash_set<int> ids{};
};

template <typename T> struct custom_codec_sequence {
  caors::vector<T> values{};
};

struct custom_codec_sequence_actor {
  custom_codec_sequence<int> scores{};
};

template <typename K, typename V> struct custom_codec_map {
  caors::hash_map<K, V> values{};
};

struct custom_codec_map_actor {
  custom_codec_map<int, caors::string> labels{};
};

struct nested_vector_actor {
  caors::vector<nested_stats> stats{};
};

struct object_ref_target : caors::object {};

struct other_object_ref_target : caors::object {};

struct object_ref_actor {
  caors::object_ref<object_ref_target> target{};
};

struct object_ref_resolver_data {
  caors::object_id id{};
  caors::object* instance = nullptr;
};

[[nodiscard]] bool reflection_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_reflection;
}

[[nodiscard]] bool serialization_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_component;
}

[[nodiscard]] bool type_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_id;
}

[[nodiscard]] caors::object_id resolve_object_id(const caors::object* instance, void* user_data) {
  const auto* data = static_cast<const object_ref_resolver_data*>(user_data);
  if (data == nullptr || instance == nullptr || instance != data->instance) {
    return caors::object_id::invalid();
  }

  return data->id;
}

[[nodiscard]] caors::object* resolve_object(caors::object_id id, void* user_data) {
  const auto* data = static_cast<const object_ref_resolver_data*>(user_data);
  if (data == nullptr || id != data->id) {
    return nullptr;
  }

  return data->instance;
}

class numeric_field_writer final : public caors::serialization_writer {
public:
  [[nodiscard]] caors::serialization_format_features features() const noexcept override {
    return caors::serialization_format_features::numeric_fields;
  }

  [[nodiscard]] caors::error begin_object(const caors::serialization_type_data&) override {
    object_open = true;
    return {};
  }

  [[nodiscard]] caors::error begin_field(const caors::serialization_field_data& field) override {
    if (field.field_id == 0) {
      return {caors::error_domain::serialization, caors::errc::invalid_field};
    }

    last_field_id = field.field_id;
    return {};
  }

  [[nodiscard]] caors::error begin_array(std::size_t) override { return {}; }
  [[nodiscard]] caors::error begin_element(std::size_t) override { return {}; }
  [[nodiscard]] caors::error write_bool(bool) override { return {}; }
  [[nodiscard]] caors::error write_i64(std::int64_t value) override {
    last_i64 = value;
    return {};
  }
  [[nodiscard]] caors::error write_u64(std::uint64_t) override { return {}; }
  [[nodiscard]] caors::error write_f64(double) override { return {}; }
  [[nodiscard]] caors::error write_string(std::string_view) override { return {}; }
  [[nodiscard]] caors::error end_element() override { return {}; }
  [[nodiscard]] caors::error end_array() override { return {}; }
  [[nodiscard]] caors::error end_field() override { return {}; }
  [[nodiscard]] caors::error end_object() override {
    object_closed = true;
    return {};
  }

  bool object_open = false;
  bool object_closed = false;
  std::uint32_t last_field_id = 0;
  std::int64_t last_i64 = 0;
};

} // namespace

namespace caors {

template <> struct type_name<unsupported_field_value> {
  static constexpr std::string_view value = "caors.test.serialization.unsupported_value";
};

template <> struct type_name<nested_stats> {
  static constexpr std::string_view value = "caors.test.serialization.nested_stats";
};

template <typename T> struct sequence_traits<custom_codec_sequence<T>> {
  static constexpr bool is_sequence = true;
  using element_type = std::remove_cv_t<T>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    return "caors.test.serialization.custom_sequence";
  }

  [[nodiscard]] static std::size_t size(const custom_codec_sequence<T>& values) noexcept {
    return values.values.size();
  }

  [[nodiscard]] static const element_type* element(const custom_codec_sequence<T>& values,
                                                   std::size_t index) noexcept {
    return index < values.values.size() ? &values.values[index] : nullptr;
  }

  [[nodiscard]] static element_type* mutable_element(custom_codec_sequence<T>& values,
                                                     std::size_t index) noexcept {
    return index < values.values.size() ? &values.values[index] : nullptr;
  }

  static void clear(custom_codec_sequence<T>& values) { values.values.clear(); }

  [[nodiscard]] static bool resize(custom_codec_sequence<T>& values, std::size_t size) {
    values.values.resize(size);
    return true;
  }

  [[nodiscard]] static bool append(custom_codec_sequence<T>& values, const element_type& value) {
    values.values.push_back(value);
    return true;
  }
};

template <typename K, typename V> struct associative_traits<custom_codec_map<K, V>> {
  static constexpr bool is_associative = true;
  using key_type = std::remove_cv_t<K>;
  using mapped_type = std::remove_cv_t<V>;
  using map_type = custom_codec_map<K, V>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    return "caors.test.serialization.custom_map";
  }

  [[nodiscard]] static std::size_t size(const map_type& values) noexcept {
    return values.values.size();
  }

  template <typename Visitor> static void for_each(const map_type& values, Visitor&& visitor) {
    for (const auto& [key, value] : values.values) {
      visitor(key, value);
    }
  }

  static void clear(map_type& values) { values.values.clear(); }

  [[nodiscard]] static bool insert(map_type& values, key_type key, mapped_type value) {
    return values.values.emplace(std::move(key), std::move(value)).second;
  }
};

template <typename T>
struct serialization_codec<custom_codec_sequence<T>>
    : sequence_serialization_codec<custom_codec_sequence<T>> {};

template <typename K, typename V>
struct serialization_codec<custom_codec_map<K, V>>
    : associative_serialization_codec<custom_codec_map<K, V>> {};

} // namespace caors

TEST_CASE("serialization - registers type metadata component", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<serializable_actor>("caors.test.serialization.actor")
          .field("id", &serializable_actor::id)
          .field("health", &serializable_actor::health)
          .field("active", &serializable_actor::active)
          .field("name", &serializable_actor::name)
          .commit()));

  const caors::error status = caors::register_serialization<serializable_actor>();

  CHECK(serialization_result_ok(status));
  CHECK(caors::get_serialization_registry().contains(caors::find_type<serializable_actor>()->id()));
  CHECK(caors::get_serialization_registry().size() >= 1);

  const caors::error duplicate = caors::register_serialization<serializable_actor>();
  CHECK(duplicate.failed());
  CHECK(duplicate.domain() == caors::error_domain::serialization);
  CHECK(duplicate.value() == caors::errc::duplicate_component);
}

TEST_CASE("serialization - serializes reflected struct fields to text", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<serializable_actor>("caors.test.serialization.actor")
          .field("id", &serializable_actor::id)
          .field("health", &serializable_actor::health)
          .field("active", &serializable_actor::active)
          .field("name", &serializable_actor::name)
          .commit()));

  const caors::error register_status = caors::register_serialization<serializable_actor>();
  REQUIRE(serialization_result_ok(register_status));

  const serializable_actor actor{7, 95.5F, true, "Ada"};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value == R"({"id":7,"health":95.5,"active":true,"name":"Ada"})");
}

TEST_CASE("serialization - serializes reflected struct fields to json", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<serializable_actor>("caors.test.serialization.actor")
          .field("id", &serializable_actor::id)
          .field("health", &serializable_actor::health)
          .field("active", &serializable_actor::active)
          .field("name", &serializable_actor::name)
          .commit()));

  const caors::error register_status = caors::register_serialization<serializable_actor>();
  REQUIRE(serialization_result_ok(register_status));

  const serializable_actor actor{7, 95.5F, true, "Ada\nLovelace"};
  const caors::serialization_text_result result = caors::serialize_json(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value == R"({"id":7,"health":95.5,"active":true,"name":"Ada\nLovelace"})");
}

TEST_CASE("serialization - field metadata renames and skips fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<configured_actor>("caors.test.serialization.configured_actor")
          .field("id", &configured_actor::id)
          .field("health", &configured_actor::health)
          .field("debug", &configured_actor::debug)
          .commit()));

  REQUIRE(caors::register_serialization<configured_actor>(
              {
                  caors::make_serialization_field("health", "hp"),
                  caors::make_serialization_field("debug", {}, true),
                  caors::make_serialization_field("id"),
              })
              .succeeded());

  const configured_actor actor{7, 42.5F, true};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value == R"({"hp":42.5,"id":7})");
}

TEST_CASE("serialization - generic writer backend serializes fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<writer_actor>("caors.test.serialization.writer_actor")
          .field("id", &writer_actor::id)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<writer_actor>({
      caors::make_serialization_field("id", {}, false, 7),
  })));

  numeric_field_writer writer;
  const writer_actor actor{42};
  const caors::error result = caors::serialize(actor, writer);

  REQUIRE(result.succeeded());
  CHECK(writer.object_open);
  CHECK(writer.object_closed);
  CHECK(writer.last_field_id == 7);
  CHECK(writer.last_i64 == 42);
}

TEST_CASE("serialization - builds variant from reflected object", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<dom_actor>("caors.test.serialization.dom_actor")
          .field("id", &dom_actor::id)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<dom_actor>({
      caors::make_serialization_field("id", "actor_id", false, 7),
  })));

  const dom_actor actor{42};
  const caors::variant_result result = caors::to_serialization_value(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value.kind() == caors::variant_kind::object);
  REQUIRE(result.value.as_object().size() == 1);
  CHECK(result.value.as_object()[0].name == "actor_id");
  CHECK(result.value.as_object()[0].field_id == 7);
  CHECK(result.value.as_object()[0].value.kind() == caors::variant_kind::signed_integer);
  CHECK(result.value.as_object()[0].value.as_i64() == 42);
}

TEST_CASE("serialization - writes intermediate arrays and objects through text writer",
          "[serialization]") {
  caors::variant::array_type items;
  items.push_back(caors::variant::from(std::int64_t{1}));
  items.push_back(caors::variant::from(std::int64_t{2}));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"items"},
      1,
      caors::variant::array(std::move(items)),
  });

  caors::text_serialization_writer writer;
  const caors::error result = caors::write(writer, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(writer.text() == R"({"items":[1,2]})");
}

TEST_CASE("serialization - variant exposes checked accessors and helpers", "[serialization]") {
  caors::variant items = caors::variant::array({});
  CHECK(items.is_array());
  CHECK(items.try_as_array() != nullptr);
  CHECK(items.try_as_object() == nullptr);
  CHECK(items.push_back(caors::variant::from(std::int64_t{1})));
  CHECK(items.push_back(caors::variant::from(std::int64_t{2})));
  REQUIRE(items.try_as_array() != nullptr);
  REQUIRE(items.try_as_array()->size() == 2);
  CHECK(items.try_as_array()->at(0).as_i64() == 1);

  caors::variant text = caors::variant::from(std::string_view{"Ada"});
  CHECK(text.is_string());
  REQUIRE(text.try_as_string() != nullptr);
  CHECK(*text.try_as_string() == "Ada");
  CHECK(text.try_as_i64() == nullptr);
  CHECK_FALSE(text.push_back(caors::variant::null()));

  caors::variant object = caors::variant::object({});
  CHECK(object.add_member(caors::string_name{"items"}, 7, std::move(items)));
  CHECK(object.find_member(caors::string_name{"items"}) != nullptr);
  CHECK(object.find_member(7) != nullptr);
  CHECK(object.find_member(caors::string_name{"missing"}) == nullptr);
  CHECK(object.find_member(99) == nullptr);
  REQUIRE(object.find_member(7) != nullptr);
  CHECK(object.find_member(7)->value.is_array());
}

TEST_CASE("serialization - converts runtime values to variant", "[serialization]") {
  caors::any number = caors::any::from(42);
  caors::variant_result number_result = caors::to_variant(number);
  REQUIRE(number_result.succeeded());
  CHECK(number_result.value.is_i64());
  REQUIRE(number_result.value.try_as_i64() != nullptr);
  CHECK(*number_result.value.try_as_i64() == 42);

  caors::string name = "Ada";
  caors::any_ref name_ref = caors::any_ref::from(name);
  caors::variant_result name_result = caors::to_variant(name_ref);
  REQUIRE(name_result.succeeded());
  CHECK(name_result.value.is_string());
  REQUIRE(name_result.value.try_as_string() != nullptr);
  CHECK(*name_result.value.try_as_string() == "Ada");

  const std::optional<caors::guid> id = caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  REQUIRE(id.has_value());
  caors::variant_result guid_result = caors::to_variant(caors::any::from(*id));
  REQUIRE(guid_result.succeeded());
  CHECK(guid_result.value.is_string());
  REQUIRE(guid_result.value.try_as_string() != nullptr);
  CHECK(*guid_result.value.try_as_string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");

  caors::variant_result invalid_result = caors::to_variant(caors::any_ref{});
  CHECK(invalid_result.failed());
  CHECK(invalid_result.status.value() == caors::errc::invalid_argument);
}

TEST_CASE("serialization - variant reader deserializes reflected struct fields",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<reader_actor>("caors.test.serialization.reader_actor")
          .field("id", &reader_actor::id)
          .field("health", &reader_actor::health)
          .field("active", &reader_actor::active)
          .field("name", &reader_actor::name)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<reader_actor>()));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{7}),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"health"},
      0,
      caors::variant::from(95.5),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"active"},
      0,
      caors::variant::from(true),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"name"},
      0,
      caors::variant::from(std::string_view{"Ada"}),
  });

  reader_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id == 7);
  CHECK(actor.health == Catch::Approx(95.5));
  CHECK(actor.active);
  CHECK(actor.name == "Ada");
}

TEST_CASE("serialization - parses json and deserializes reflected struct fields",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<reader_actor>("caors.test.serialization.reader_actor")
          .field("id", &reader_actor::id)
          .field("health", &reader_actor::health)
          .field("active", &reader_actor::active)
          .field("name", &reader_actor::name)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<reader_actor>()));

  const caors::variant_result parsed =
      caors::parse_json(R"({"id":7,"health":95.5,"active":true,"name":"Ada\nLovelace"})");
  REQUIRE(parsed.succeeded());
  REQUIRE(parsed.value.is_object());
  const caors::variant_member* name = parsed.value.find_member(caors::string_name{"name"});
  REQUIRE(name != nullptr);
  REQUIRE(name->value.is_string());
  CHECK(name->value.as_string() == "Ada\nLovelace");

  reader_actor actor{};
  const caors::error result =
      caors::deserialize_json(actor, R"({"id":7,"health":95.5,"active":true,"name":"Ada"})");

  REQUIRE(result.succeeded());
  CHECK(actor.id == 7);
  CHECK(actor.health == Catch::Approx(95.5));
  CHECK(actor.active);
  CHECK(actor.name == "Ada");
}

TEST_CASE("serialization - rejects invalid json", "[serialization]") {
  const caors::variant_result parsed = caors::parse_json(R"({"id":1,})");

  CHECK(parsed.failed());
  CHECK(parsed.status.domain() == caors::error_domain::serialization);
  CHECK(parsed.status.value() == caors::errc::invalid_argument);
}

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
  REQUIRE(serialization_result_ok(caors::register_serialization<nested_stats>()));
  REQUIRE(serialization_result_ok(caors::register_serialization<nested_actor>()));

  const nested_actor actor{7, nested_stats{3, 12.5F}};
  const caors::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"id":7,"stats":{"level":3,"score":12.5}})");

  const caors::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());
  const caors::variant_member* stats = value.value.find_member(caors::string_name{"stats"});
  REQUIRE(stats != nullptr);
  REQUIRE(stats->value.is_object());
  const caors::variant_member* level = stats->value.find_member(caors::string_name{"level"});
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
  REQUIRE(serialization_result_ok(caors::register_serialization<nested_stats>()));
  REQUIRE(
      serialization_result_ok(caors::register_serialization_codec<caors::vector<nested_stats>>()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<nested_vector_actor>("caors.test.serialization.nested_vector_actor")
          .field("stats", &nested_vector_actor::stats)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<nested_vector_actor>()));

  nested_vector_actor actor{};
  actor.stats.push_back(nested_stats{1, 2.5F});
  actor.stats.push_back(nested_stats{3, 4.5F});

  const caors::serialization_text_result json = caors::serialize_json(actor);
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
  REQUIRE(serialization_result_ok(caors::register_serialization<nested_stats>()));
  REQUIRE(serialization_result_ok(caors::register_serialization<nested_actor>()));

  caors::variant::object_type stats_members;
  stats_members.push_back(caors::variant_member{
      caors::string_name{"level"},
      0,
      caors::variant::from(std::int64_t{4}),
  });
  stats_members.push_back(caors::variant_member{
      caors::string_name{"score"},
      0,
      caors::variant::from(15.25),
  });

  caors::variant::object_type actor_members;
  actor_members.push_back(caors::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{9}),
  });
  actor_members.push_back(caors::variant_member{
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

TEST_CASE("serialization - serializes vector fields as arrays", "[serialization]") {
  REQUIRE(serialization_result_ok(caors::register_serialization_codec<caors::vector<int>>()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<vector_actor>("caors.test.serialization.vector_actor")
          .field("scores", &vector_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<vector_actor>()));

  vector_actor actor{};
  actor.scores.push_back(1);
  actor.scores.push_back(2);
  actor.scores.push_back(3);

  const caors::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[1,2,3]})");

  const caors::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::variant_member* scores = value.value.find_member(caors::string_name{"scores"});
  REQUIRE(scores != nullptr);
  REQUIRE(scores->value.is_array());
  REQUIRE(scores->value.as_array().size() == 3);
  CHECK(scores->value.as_array()[2].as_i64() == 3);
}

TEST_CASE("serialization - deserializes vector fields from arrays", "[serialization]") {
  REQUIRE(serialization_result_ok(caors::register_serialization_codec<caors::vector<int>>()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<vector_actor>("caors.test.serialization.vector_actor")
          .field("scores", &vector_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<vector_actor>()));

  caors::variant::array_type scores;
  scores.push_back(caors::variant::from(std::int64_t{5}));
  scores.push_back(caors::variant::from(std::int64_t{6}));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
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

TEST_CASE("serialization - serializes list fields as arrays", "[serialization]") {
  REQUIRE(serialization_result_ok(caors::register_serialization_codec<caors::list<int>>()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<list_actor>("caors.test.serialization.list_actor")
          .field("scores", &list_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<list_actor>()));

  list_actor actor{};
  actor.scores.push_back(1);
  actor.scores.push_back(2);
  actor.scores.push_back(3);

  const caors::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[1,2,3]})");

  const caors::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::variant_member* scores = value.value.find_member(caors::string_name{"scores"});
  REQUIRE(scores != nullptr);
  REQUIRE(scores->value.is_array());
  REQUIRE(scores->value.as_array().size() == 3);
  CHECK(scores->value.as_array()[2].as_i64() == 3);
}

TEST_CASE("serialization - deserializes list fields from arrays", "[serialization]") {
  REQUIRE(serialization_result_ok(caors::register_serialization_codec<caors::list<int>>()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<list_actor>("caors.test.serialization.list_actor")
          .field("scores", &list_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<list_actor>()));

  caors::variant::array_type scores;
  scores.push_back(caors::variant::from(std::int64_t{5}));
  scores.push_back(caors::variant::from(std::int64_t{6}));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
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

TEST_CASE("serialization - serializes hash map fields as dictionaries", "[serialization]") {
  using labels_type = caors::hash_map<int, caors::string>;
  REQUIRE(serialization_result_ok(caors::register_serialization_codec<labels_type>()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<map_actor>("caors.test.serialization.map_actor")
          .field("labels", &map_actor::labels)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<map_actor>()));

  map_actor actor{};
  actor.labels.emplace(7, caors::string{"seven"});

  const caors::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"labels":[[7,"seven"]]})");

  const caors::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::variant_member* labels = value.value.find_member(caors::string_name{"labels"});
  REQUIRE(labels != nullptr);
  REQUIRE(labels->value.is_dictionary());
  REQUIRE(labels->value.as_dictionary().size() == 1);
  CHECK(labels->value.as_dictionary()[0].key.as_i64() == 7);
  CHECK(labels->value.as_dictionary()[0].value.as_string() == "seven");
}

TEST_CASE("serialization - deserializes hash map fields from dictionaries", "[serialization]") {
  using labels_type = caors::hash_map<int, caors::string>;
  REQUIRE(serialization_result_ok(caors::register_serialization_codec<labels_type>()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<map_actor>("caors.test.serialization.map_actor")
          .field("labels", &map_actor::labels)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<map_actor>()));

  caors::variant::dictionary_type entries;
  entries.push_back(caors::variant_entry{
      caors::variant::from(std::int64_t{1}),
      caors::variant::from(std::string_view{"one"}),
  });
  entries.push_back(caors::variant_entry{
      caors::variant::from(std::int64_t{2}),
      caors::variant::from(std::string_view{"two"}),
  });

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
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
  REQUIRE(serialization_result_ok(caors::register_serialization_codec<labels_type>()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<map_actor>("caors.test.serialization.map_actor")
          .field("labels", &map_actor::labels)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<map_actor>()));

  const caors::variant_result parsed = caors::parse_json(R"({"labels":[[3,"three"]]})");
  REQUIRE(parsed.succeeded());

  map_actor actor{};
  const caors::error result = caors::deserialize(actor, parsed.value);

  REQUIRE(result.succeeded());
  REQUIRE(actor.labels.size() == 1);
  CHECK(actor.labels.at(3) == "three");
}

TEST_CASE("serialization - serializes hash set fields as arrays", "[serialization]") {
  using ids_type = caors::hash_set<int>;
  REQUIRE(serialization_result_ok(caors::register_serialization_codec<ids_type>()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<set_actor>("caors.test.serialization.set_actor")
          .field("ids", &set_actor::ids)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<set_actor>()));

  set_actor actor{};
  actor.ids.emplace(7);

  const caors::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"ids":[7]})");

  const caors::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::variant_member* ids = value.value.find_member(caors::string_name{"ids"});
  REQUIRE(ids != nullptr);
  REQUIRE(ids->value.is_array());
  REQUIRE(ids->value.as_array().size() == 1);
  CHECK(ids->value.as_array()[0].as_i64() == 7);
}

TEST_CASE("serialization - deserializes hash set fields from arrays", "[serialization]") {
  using ids_type = caors::hash_set<int>;
  REQUIRE(serialization_result_ok(caors::register_serialization_codec<ids_type>()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<set_actor>("caors.test.serialization.set_actor")
          .field("ids", &set_actor::ids)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<set_actor>()));

  caors::variant::array_type ids;
  ids.push_back(caors::variant::from(std::int64_t{1}));
  ids.push_back(caors::variant::from(std::int64_t{2}));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
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
  REQUIRE(serialization_result_ok(caors::register_serialization_codec<ids_type>()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<set_actor>("caors.test.serialization.set_actor")
          .field("ids", &set_actor::ids)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<set_actor>()));

  caors::variant::array_type ids;
  ids.push_back(caors::variant::from(std::int64_t{1}));
  ids.push_back(caors::variant::from(std::int64_t{1}));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
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

TEST_CASE("serialization - generic sequence codec uses sequence traits", "[serialization]") {
  using scores_type = custom_codec_sequence<int>;
  REQUIRE(serialization_result_ok(caors::register_serialization_codec<scores_type>()));
  REQUIRE(reflection_result_ok(caors::reflection::class_<custom_codec_sequence_actor>(
                                   "caors.test.serialization.custom_codec_sequence_actor")
                                   .field("scores", &custom_codec_sequence_actor::scores)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<custom_codec_sequence_actor>()));

  custom_codec_sequence_actor actor{};
  actor.scores.values.push_back(4);
  actor.scores.values.push_back(5);

  const caors::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[4,5]})");

  caors::variant::array_type values;
  values.push_back(caors::variant::from(std::int64_t{8}));
  values.push_back(caors::variant::from(std::int64_t{9}));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
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
  REQUIRE(serialization_result_ok(caors::register_serialization_codec<labels_type>()));
  REQUIRE(reflection_result_ok(caors::reflection::class_<custom_codec_map_actor>(
                                   "caors.test.serialization.custom_codec_map_actor")
                                   .field("labels", &custom_codec_map_actor::labels)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<custom_codec_map_actor>()));

  custom_codec_map_actor actor{};
  actor.labels.values.emplace(6, caors::string{"six"});

  const caors::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"labels":[[6,"six"]]})");

  caors::variant::dictionary_type entries;
  entries.push_back(caors::variant_entry{
      caors::variant::from(std::int64_t{10}),
      caors::variant::from(std::string_view{"ten"}),
  });

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
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
  REQUIRE(serialization_result_ok(caors::register_serialization<nested_stats>()));
  REQUIRE(
      serialization_result_ok(caors::register_serialization_codec<caors::vector<nested_stats>>()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<nested_vector_actor>("caors.test.serialization.nested_vector_actor")
          .field("stats", &nested_vector_actor::stats)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<nested_vector_actor>()));

  nested_vector_actor actor{};
  actor.stats.push_back(nested_stats{1, 2.5F});
  actor.stats.push_back(nested_stats{3, 4.5F});

  const caors::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"stats":[{"level":1,"score":2.5},{"level":3,"score":4.5}]})");

  caors::variant::object_type first_stats;
  first_stats.push_back(caors::variant_member{
      caors::string_name{"level"},
      0,
      caors::variant::from(std::int64_t{7}),
  });
  first_stats.push_back(caors::variant_member{
      caors::string_name{"score"},
      0,
      caors::variant::from(8.5),
  });

  caors::variant::array_type stats_array;
  stats_array.push_back(caors::variant::object(std::move(first_stats)));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
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

TEST_CASE("serialization - serializes object references through resolver", "[serialization]") {
  REQUIRE(type_result_ok(caors::register_type<caors::object_ref<object_ref_target>>(
      "caors.object_ref<caors.test.serialization.object_ref_target>")));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ref_actor>("caors.test.serialization.object_ref_actor")
          .field("target", &object_ref_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<object_ref_actor>()));

  object_ref_target target;
  object_ref_resolver_data resolver_data{caors::object_id{77}, &target};
  const caors::serialization_reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };
  const caors::serialization_context context{0, &resolver};
  const object_ref_actor actor{caors::object_ref<object_ref_target>{&target}};

  const caors::serialization_text_result text = caors::serialize_text(actor, context);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"target":77})");

  const caors::variant_result value = caors::to_serialization_value(actor, context);
  REQUIRE(value.succeeded());
  const caors::variant_member* target_member =
      value.value.find_member(caors::string_name{"target"});
  REQUIRE(target_member != nullptr);
  CHECK(target_member->value.is_u64());
  CHECK(target_member->value.as_u64() == 77);

  const caors::serialization_text_result json = caors::serialize_json(actor, context);
  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"target":77})");
}

TEST_CASE("serialization - deserializes object references through resolver", "[serialization]") {
  REQUIRE(type_result_ok(caors::register_type<caors::object_ref<object_ref_target>>(
      "caors.object_ref<caors.test.serialization.object_ref_target>")));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ref_actor>("caors.test.serialization.object_ref_actor")
          .field("target", &object_ref_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<object_ref_actor>()));

  object_ref_target target;
  object_ref_resolver_data resolver_data{caors::object_id{88}, &target};
  const caors::serialization_reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"target"},
      0,
      caors::variant::from(std::uint64_t{88}),
  });

  object_ref_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)),
                                                 caors::serialization_context{0, &resolver});

  REQUIRE(result.succeeded());
  CHECK(actor.target.get() == &target);

  object_ref_actor from_json{};
  const caors::error json_result = caors::deserialize_json(
      from_json, R"({"target":88})", caors::serialization_context{0, &resolver});
  REQUIRE(json_result.succeeded());
  CHECK(from_json.target.get() == &target);
}

TEST_CASE("serialization - object reference resolver failures are reported", "[serialization]") {
  REQUIRE(type_result_ok(caors::register_type<caors::object_ref<object_ref_target>>(
      "caors.object_ref<caors.test.serialization.object_ref_target>")));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ref_actor>("caors.test.serialization.object_ref_actor")
          .field("target", &object_ref_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<object_ref_actor>()));

  object_ref_target target;
  const object_ref_actor actor{caors::object_ref<object_ref_target>{&target}};
  const caors::serialization_text_result missing_resolver = caors::serialize_text(actor);
  CHECK(missing_resolver.failed());
  CHECK(missing_resolver.status.value() == caors::errc::invalid_argument);

  object_ref_resolver_data resolver_data{caors::object_id{99}, nullptr};
  const caors::serialization_reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"target"},
      0,
      caors::variant::from(std::uint64_t{99}),
  });

  object_ref_actor decoded{};
  const caors::error missing_object =
      caors::deserialize(decoded, caors::variant::object(std::move(members)),
                         caors::serialization_context{0, &resolver});
  CHECK(missing_object.failed());
  CHECK(missing_object.value() == caors::errc::not_found);

  other_object_ref_target other;
  object_ref_resolver_data wrong_type_data{caors::object_id{100}, &other};
  const caors::serialization_reference_resolver wrong_type_resolver{
      &resolve_object_id,
      &resolve_object,
      &wrong_type_data,
  };

  caors::variant::object_type wrong_type_members;
  wrong_type_members.push_back(caors::variant_member{
      caors::string_name{"target"},
      0,
      caors::variant::from(std::uint64_t{100}),
  });

  const caors::error wrong_type =
      caors::deserialize(decoded, caors::variant::object(std::move(wrong_type_members)),
                         caors::serialization_context{0, &wrong_type_resolver});
  CHECK(wrong_type.failed());
  CHECK(wrong_type.value() == caors::errc::invalid_type);
}

TEST_CASE("serialization - variant reader matches fields by stable field id", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<reader_field_id_actor>(
                                   "caors.test.serialization.reader_field_id_actor")
                                   .field("id", &reader_field_id_actor::id)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<reader_field_id_actor>({
      caors::make_serialization_field("id", "actor_id", false, 7),
  })));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"legacy_id"},
      7,
      caors::variant::from(std::int64_t{42}),
  });

  reader_field_id_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id == 42);
}

TEST_CASE("serialization - variant reader reports missing fields", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<reader_missing_field_actor>(
                                   "caors.test.serialization.reader_missing_field_actor")
                                   .field("id", &reader_missing_field_actor::id)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<reader_missing_field_actor>()));

  reader_missing_field_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object({}));

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::not_found);
}

TEST_CASE("serialization - variant reader applies required optional and skipped policies",
          "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<reader_field_policy_actor>(
                                   "caors.test.serialization.reader_field_policy_actor")
                                   .field("id", &reader_field_policy_actor::id)
                                   .field("optional", &reader_field_policy_actor::optional)
                                   .field("required", &reader_field_policy_actor::required)
                                   .field("skipped", &reader_field_policy_actor::skipped)
                                   .commit()));

  caors::serialization_field_data id = caors::make_serialization_field("id");
  caors::serialization_field_data optional = caors::make_serialization_field("optional");
  caors::serialization_field_data required = caors::make_serialization_field("required");
  required.required = true;
  caors::serialization_field_data skipped = caors::make_serialization_field("skipped");
  skipped.skipped = true;

  REQUIRE(serialization_result_ok(caors::register_serialization<reader_field_policy_actor>({
      id,
      optional,
      required,
      skipped,
  })));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{1}),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"required"},
      0,
      caors::variant::from(std::int64_t{3}),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"skipped"},
      0,
      caors::variant::from(std::int64_t{4}),
  });

  reader_field_policy_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id == 1);
  CHECK(actor.optional == 20);
  CHECK(actor.required == 3);
  CHECK(actor.skipped == 40);
}

TEST_CASE("serialization - variant reader reports missing required fields", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<reader_field_policy_actor>(
                                   "caors.test.serialization.reader_field_policy_actor")
                                   .field("id", &reader_field_policy_actor::id)
                                   .field("optional", &reader_field_policy_actor::optional)
                                   .field("required", &reader_field_policy_actor::required)
                                   .field("skipped", &reader_field_policy_actor::skipped)
                                   .commit()));

  caors::serialization_field_data id = caors::make_serialization_field("id");
  caors::serialization_field_data optional = caors::make_serialization_field("optional");
  caors::serialization_field_data required = caors::make_serialization_field("required");
  required.required = true;
  caors::serialization_field_data skipped = caors::make_serialization_field("skipped");
  skipped.skipped = true;

  REQUIRE(serialization_result_ok(caors::register_serialization<reader_field_policy_actor>({
      id,
      optional,
      required,
      skipped,
  })));

  reader_field_policy_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object({}));

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::not_found);
}

TEST_CASE("serialization - variant reader ignores unknown object members", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<reader_unknown_field_actor>(
                                   "caors.test.serialization.reader_unknown_field_actor")
                                   .field("id", &reader_unknown_field_actor::id)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<reader_unknown_field_actor>({
      caors::make_serialization_field("id"),
  })));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{7}),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"extra"},
      0,
      caors::variant::from(std::int64_t{99}),
  });

  reader_unknown_field_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id == 7);
}

TEST_CASE("serialization - context filters fields by schema version", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<versioned_actor>("caors.test.serialization.versioned_actor")
          .field("id", &versioned_actor::id)
          .field("legacy", &versioned_actor::legacy)
          .field("current", &versioned_actor::current)
          .field("future", &versioned_actor::future)
          .commit()));

  caors::serialization_field_data id = caors::make_serialization_field("id");
  caors::serialization_field_data legacy = caors::make_serialization_field("legacy");
  legacy.version_added = 1;
  legacy.version_removed = 2;
  caors::serialization_field_data current = caors::make_serialization_field("current");
  current.version_added = 2;
  caors::serialization_field_data future = caors::make_serialization_field("future");
  future.version_added = 3;

  REQUIRE(serialization_result_ok(caors::register_serialization<versioned_actor>({
      id,
      legacy,
      current,
      future,
  })));

  const versioned_actor actor{1, 10, 20, 30};
  const caors::serialization_text_result text =
      caors::serialize_text(actor, caors::serialization_context{2});

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"id":1,"current":20})");

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{2}),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"current"},
      0,
      caors::variant::from(std::int64_t{22}),
  });

  versioned_actor deserialized{};
  const caors::error result = caors::deserialize(
      deserialized, caors::variant::object(std::move(members)), caors::serialization_context{2});

  REQUIRE(result.succeeded());
  CHECK(deserialized.id == 2);
  CHECK(deserialized.legacy == 0);
  CHECK(deserialized.current == 22);
  CHECK(deserialized.future == 0);
}

TEST_CASE("serialization - built in codecs serialize guid and type guid as scalar strings",
          "[serialization]") {
  const std::optional<caors::guid> id = caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  const std::optional<caors::guid> type =
      caors::guid::parse("5ed39a97-7b20-4d87-86ac-e62a4a615b10");
  REQUIRE(id.has_value());
  REQUIRE(type.has_value());

  REQUIRE(reflection_result_ok(
      caors::reflection::class_<codec_guid_actor>("caors.test.serialization.codec_guid_actor")
          .field("id", &codec_guid_actor::id)
          .field("type", &codec_guid_actor::type)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<codec_guid_actor>()));

  const codec_guid_actor actor{*id, caors::type_guid{*type}};
  const caors::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(
      text.value ==
      R"({"id":"2f6d1f64-7a3a-4db2-9a2d-123456789abc","type":"5ed39a97-7b20-4d87-86ac-e62a4a615b10"})");

  const caors::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.as_object().size() == 2);
  CHECK(value.value.as_object()[0].value.kind() == caors::variant_kind::string);
  CHECK(value.value.as_object()[0].value.as_string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  CHECK(value.value.as_object()[1].value.kind() == caors::variant_kind::string);
  CHECK(value.value.as_object()[1].value.as_string() == "5ed39a97-7b20-4d87-86ac-e62a4a615b10");
}

TEST_CASE("serialization - built in codecs deserialize guid and type guid from strings",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<codec_guid_actor>("caors.test.serialization.codec_guid_actor")
          .field("id", &codec_guid_actor::id)
          .field("type", &codec_guid_actor::type)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<codec_guid_actor>()));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::string_view{"2f6d1f64-7a3a-4db2-9a2d-123456789abc"}),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"type"},
      0,
      caors::variant::from(std::string_view{"5ed39a97-7b20-4d87-86ac-e62a4a615b10"}),
  });

  codec_guid_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id.string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  CHECK(actor.type.value.string() == "5ed39a97-7b20-4d87-86ac-e62a4a615b10");
}

TEST_CASE("serialization - built in codecs reject invalid guid text", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<codec_guid_actor>("caors.test.serialization.codec_guid_actor")
          .field("id", &codec_guid_actor::id)
          .field("type", &codec_guid_actor::type)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<codec_guid_actor>()));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::string_view{"invalid-guid"}),
  });

  codec_guid_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_argument);
}

TEST_CASE("serialization - built in codecs serialize string view fields", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<codec_string_view_actor>(
                                   "caors.test.serialization.codec_string_view_actor")
                                   .field("name", &codec_string_view_actor::name)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<codec_string_view_actor>()));

  const codec_string_view_actor actor{std::string_view{"Ada"}};
  const caors::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"name":"Ada"})");
}

TEST_CASE("serialization - writer features validate required field identifiers",
          "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<numeric_writer_actor>(
                                   "caors.test.serialization.numeric_writer_actor")
                                   .field("id", &numeric_writer_actor::id)
                                   .commit()));
  REQUIRE(
      caors::register_serialization<numeric_writer_actor>({
                                                              caors::make_serialization_field("id"),
                                                          })
          .succeeded());

  numeric_field_writer writer;
  const numeric_writer_actor actor{5};
  const caors::error result = caors::serialize(actor, writer);

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_field);
}

TEST_CASE("serialization - registration can happen before reflection fields", "[serialization]") {
  REQUIRE(caors::register_serialization<serialization_first_actor>(
              {
                  caors::make_serialization_field("id", "actor_id"),
              })
              .succeeded());

  REQUIRE(reflection_result_ok(caors::reflection::class_<serialization_first_actor>(
                                   "caors.test.serialization.serialization_first_actor")
                                   .field("id", &serialization_first_actor::id)
                                   .commit()));

  const serialization_first_actor actor{9};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value == R"({"actor_id":9})");
}

TEST_CASE("serialization - reports missing serialization metadata", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<reflected_only_actor>("caors.test.serialization.reflected_only")
          .field("id", &reflected_only_actor::id)
          .commit()));

  const reflected_only_actor actor{3};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::missing_type);
  CHECK(result.value.empty());
}

TEST_CASE("serialization - reports missing configured reflection field", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<missing_field_actor>("caors.test.serialization.missing_field")
          .field("id", &missing_field_actor::id)
          .commit()));
  REQUIRE(caors::register_serialization<missing_field_actor>(
              {
                  caors::make_serialization_field("missing"),
              })
              .succeeded());

  const missing_field_actor actor{3};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::not_found);
}

TEST_CASE("serialization - reports unsupported reflected field kind", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<unsupported_field_actor>(
                                   "caors.test.serialization.unsupported_field")
                                   .field("value", &unsupported_field_actor::value)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<unsupported_field_actor>()));

  const unsupported_field_actor actor{{1}};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::unsupported);
}
