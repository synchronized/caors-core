// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection/builder.h"
#include "caors/serialization.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <forward_list>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace {

struct serializable_actor {
  int id = 0;
  float health = 0.0F;
  bool active = false;
  caors::string name{};
};

struct explicit_meta_serialization_actor {
  int id = 0;
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

struct codec_path_actor {
  std::filesystem::path path{};
};

struct codec_chrono_actor {
  std::chrono::milliseconds timeout{};
  std::chrono::system_clock::time_point timestamp{};
};

enum class scalar_color : std::uint8_t {
  red = 1,
  blue = 2,
};

enum class scalar_state : int {
  disabled = -1,
  enabled = 2,
};

struct scalar_actor {
  char code = '\0';
  scalar_color color = scalar_color::red;
  scalar_state state = scalar_state::disabled;
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

struct weak_ptr_actor {
  std::weak_ptr<int> value{};
};

struct nested_stats {
  int level = 0;
  float score = 0.0F;
};

struct nested_actor {
  int id = 0;
  nested_stats stats{};
};

struct non_default_value {
  explicit non_default_value(int value) : value(value) {}

  int value = 0;
};

struct non_default_field_actor {
  non_default_value value{1};
};

struct non_default_vector_actor {
  caors::vector<non_default_value> values{};
};

struct non_default_unique_ptr_actor {
  std::unique_ptr<non_default_value> value{};
};

struct non_default_variant_actor {
  std::variant<int, non_default_value> value{0};
};

struct pointer_actor {
  int* raw = nullptr;
  std::shared_ptr<int> shared{};
  std::unique_ptr<int> unique{};
  std::optional<int> optional{};
  nested_stats* stats = nullptr;
};

struct reference_wrapper_actor {
  reference_wrapper_actor() : value(storage) {}
  explicit reference_wrapper_actor(int initial) : storage(initial), value(storage) {}

  int storage = 0;
  std::reference_wrapper<int> value;
};

struct vector_actor {
  caors::vector<int> scores{};
};

struct deque_actor {
  std::deque<int> scores{};
};

struct array_actor {
  std::array<int, 3> scores{};
};

struct span_actor {
  std::span<int> scores{};
};

struct pair_actor {
  std::pair<int, caors::string> entry{};
};

struct tuple_actor {
  std::tuple<int, caors::string, float> entry{};
};

struct variant_actor {
  std::variant<int, caors::string, float> entry{};
};

struct nested_vector_values_actor {
  caors::vector<caors::vector<int>> groups{};
};

struct unique_ptr_vector_actor {
  unique_ptr_vector_actor() = default;
  unique_ptr_vector_actor(const unique_ptr_vector_actor&) = delete;
  unique_ptr_vector_actor& operator=(const unique_ptr_vector_actor&) = delete;
  unique_ptr_vector_actor(unique_ptr_vector_actor&&) noexcept = default;
  unique_ptr_vector_actor& operator=(unique_ptr_vector_actor&&) noexcept = default;

  caors::vector<std::unique_ptr<int>> values{};
};

struct guid_vector_actor {
  caors::vector<caors::guid> ids{};
};

struct list_actor {
  caors::list<int> scores{};
};

struct forward_list_actor {
  std::forward_list<int> scores{};
};

struct map_actor {
  caors::hash_map<int, caors::string> labels{};
};

struct set_actor {
  caors::hash_set<int> ids{};
};

struct ordered_set_actor {
  std::set<int> ids{};
};

struct ordered_map_actor {
  std::map<int, caors::string> labels{};
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

struct object_ref_vector_actor {
  caors::vector<caors::object_ref<object_ref_target>> targets{};
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

template <typename T> void require_registered_type(std::string_view name) {
  REQUIRE(type_result_ok(caors::detail::register_type<T>(name)));
}

template <typename T> void require_serialization_codec(std::string_view name) {
  require_registered_type<T>(name);
  REQUIRE(serialization_result_ok(caors::register_serialization_codec<T>()));
}

void require_pointer_actor_metadata() {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<nested_stats>("caors.test.serialization.nested_stats")
          .field("level", &nested_stats::level)
          .field("score", &nested_stats::score)
          .commit()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<pointer_actor>("caors.test.serialization.pointer_actor")
          .field("raw", &pointer_actor::raw, "int*")
          .field("shared", &pointer_actor::shared, "std.shared_ptr<int>")
          .field("unique", &pointer_actor::unique, "std.unique_ptr<int>")
          .field("optional", &pointer_actor::optional, "std.optional<int>")
          .field("stats", &pointer_actor::stats, "caors.test.serialization.nested_stats*")
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<nested_stats>()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<pointer_actor>()));
}

void require_non_default_value_metadata() {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<non_default_value>("caors.test.serialization.non_default_value")
          .field("value", &non_default_value::value)
          .commit()));
  REQUIRE(reflection_result_ok(caors::reflection::class_<non_default_field_actor>(
                                   "caors.test.serialization.non_default_field_actor")
                                   .field("value", &non_default_field_actor::value)
                                   .commit()));
  REQUIRE(reflection_result_ok(caors::reflection::class_<non_default_vector_actor>(
                                   "caors.test.serialization.non_default_vector_actor")
                                   .field("values", &non_default_vector_actor::values)
                                   .commit()));
  REQUIRE(
      reflection_result_ok(caors::reflection::class_<non_default_unique_ptr_actor>(
                               "caors.test.serialization.non_default_unique_ptr_actor")
                               .field("value", &non_default_unique_ptr_actor::value,
                                      "std.unique_ptr<caors.test.serialization.non_default_value>")
                               .commit()));
  REQUIRE(reflection_result_ok(caors::reflection::class_<non_default_variant_actor>(
                                   "caors.test.serialization.non_default_variant_actor")
                                   .field("value", &non_default_variant_actor::value)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<non_default_value>()));
  REQUIRE(
      serialization_result_ok(caors::detail::register_serialization<non_default_field_actor>()));
  REQUIRE(
      serialization_result_ok(caors::detail::register_serialization<non_default_vector_actor>()));
  REQUIRE(serialization_result_ok(
      caors::detail::register_serialization<non_default_unique_ptr_actor>()));
  REQUIRE(
      serialization_result_ok(caors::detail::register_serialization<non_default_variant_actor>()));
}

void require_unique_ptr_vector_actor_metadata() {
  REQUIRE(
      type_result_ok(caors::detail::register_type<std::unique_ptr<int>>("std.unique_ptr<int>")));
  REQUIRE(reflection_result_ok(caors::reflection::class_<unique_ptr_vector_actor>(
                                   "caors.test.serialization.unique_ptr_vector_actor")
                                   .field("values", &unique_ptr_vector_actor::values)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::detail::register_serialization<unique_ptr_vector_actor>()));
}

void require_scalar_actor_metadata() {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<scalar_actor>("caors.test.serialization.scalar_actor")
          .field("code", &scalar_actor::code)
          .field("color", &scalar_actor::color, "caors.test.serialization.scalar_color")
          .field("state", &scalar_actor::state, "caors.test.serialization.scalar_state")
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<scalar_actor>()));
}

void require_object_ref_vector_actor_metadata() {
  REQUIRE(type_result_ok(caors::detail::register_type<caors::object_ref<object_ref_target>>(
      "caors.object_ref<caors.test.serialization.object_ref_target>")));
  REQUIRE(reflection_result_ok(caors::reflection::class_<object_ref_vector_actor>(
                                   "caors.test.serialization.object_ref_vector_actor")
                                   .field("targets", &object_ref_vector_actor::targets)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::detail::register_serialization<object_ref_vector_actor>()));
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

  const caors::error status = caors::detail::register_serialization<serializable_actor>();

  CHECK(serialization_result_ok(status));
  CHECK(caors::get_serialization_registry().contains(caors::find_type<serializable_actor>()->id()));
  CHECK(caors::get_serialization_registry().size() >= 1);

  const caors::error duplicate = caors::detail::register_serialization<serializable_actor>();
  CHECK(duplicate.failed());
  CHECK(duplicate.domain() == caors::error_domain::serialization);
  CHECK(duplicate.value() == caors::errc::duplicate_component);
}

TEST_CASE("serialization - class builder can attach to explicit meta type", "[serialization]") {
  const caors::meta_type type =
      caors::meta<explicit_meta_serialization_actor>("caors.test.serialization.explicit_meta_actor")
          .get();
  REQUIRE(type.valid());
  CHECK(caors::type::get<explicit_meta_serialization_actor>().id() == type.id());

  REQUIRE(reflection_result_ok(caors::reflection::class_<explicit_meta_serialization_actor>(type)
                                   .field("id", &explicit_meta_serialization_actor::id)
                                   .commit()));

  const caors::error status = caors::serialization::class_(type).field("id").commit();
  REQUIRE(serialization_result_ok(status));

  const explicit_meta_serialization_actor actor{42};
  const caors::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"id":42})");
}

TEST_CASE("serialization - explicit registration reports missing meta type", "[serialization]") {
  const caors::error status = caors::detail::register_serialization(
      caors::type_id{std::numeric_limits<std::uint64_t>::max()});

  CHECK(status.failed());
  CHECK(status.domain() == caors::error_domain::serialization);
  CHECK(status.value() == caors::errc::missing_type);
}

TEST_CASE("serialization - serializes reflected struct fields to text", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<serializable_actor>("caors.test.serialization.actor")
          .field("id", &serializable_actor::id)
          .field("health", &serializable_actor::health)
          .field("active", &serializable_actor::active)
          .field("name", &serializable_actor::name)
          .commit()));

  const caors::error register_status = caors::detail::register_serialization<serializable_actor>();
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

  const caors::error register_status = caors::detail::register_serialization<serializable_actor>();
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

  REQUIRE(caors::detail::register_serialization<configured_actor>(
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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<writer_actor>({
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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<dom_actor>({
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
  require_registered_type<int>("int");
  require_registered_type<caors::string>("caors.string");
  require_registered_type<caors::guid>("caors.guid");

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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<reader_actor>()));

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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<reader_actor>()));

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

TEST_CASE("serialization - roundtrips char and enum fields", "[serialization]") {
  require_scalar_actor_metadata();

  scalar_actor actor{};
  actor.code = 'A';
  actor.color = scalar_color::blue;
  actor.state = scalar_state::enabled;

  const caors::serialization_text_result text = caors::serialize_json(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"code":"A","color":2,"state":2})");

  const caors::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());

  const caors::variant_member* code = value.value.find_member(caors::string_name{"code"});
  const caors::variant_member* color = value.value.find_member(caors::string_name{"color"});
  const caors::variant_member* state = value.value.find_member(caors::string_name{"state"});
  REQUIRE(code != nullptr);
  REQUIRE(color != nullptr);
  REQUIRE(state != nullptr);
  REQUIRE(code->value.try_as_string() != nullptr);
  REQUIRE(color->value.try_as_u64() != nullptr);
  REQUIRE(state->value.try_as_i64() != nullptr);
  CHECK(code->value.as_string() == "A");
  CHECK(color->value.as_u64() == 2U);
  CHECK(state->value.as_i64() == 2);

  scalar_actor decoded{};
  const caors::error result =
      caors::deserialize_json(decoded, R"({"code":"Z","color":1,"state":-1})");
  REQUIRE(result.succeeded());
  CHECK(decoded.code == 'Z');
  CHECK(decoded.color == scalar_color::red);
  CHECK(decoded.state == scalar_state::disabled);
}

TEST_CASE("serialization - rejects invalid char and enum values", "[serialization]") {
  require_scalar_actor_metadata();

  scalar_actor actor{};

  const caors::error invalid_char =
      caors::deserialize_json(actor, R"({"code":"AB","color":1,"state":-1})");
  REQUIRE(invalid_char.failed());
  CHECK(invalid_char.domain() == caors::error_domain::serialization);

  const caors::error invalid_enum =
      caors::deserialize_json(actor, R"({"code":"A","color":300,"state":-1})");
  REQUIRE(invalid_enum.failed());
  CHECK(invalid_enum.domain() == caors::error_domain::serialization);
  CHECK(invalid_enum.value() == caors::errc::invalid_argument);
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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<nested_stats>()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<nested_actor>()));

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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<nested_stats>()));
  require_serialization_codec<caors::vector<nested_stats>>(
      "caors.vector<caors.test.serialization.nested_stats>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<nested_vector_actor>("caors.test.serialization.nested_vector_actor")
          .field("stats", &nested_vector_actor::stats)
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<nested_vector_actor>()));

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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<nested_stats>()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<nested_actor>()));

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

TEST_CASE("serialization - updates non default constructible direct fields", "[serialization]") {
  require_non_default_value_metadata();

  non_default_field_actor actor{non_default_value{1}};

  const caors::serialization_text_result text = caors::serialize_text(actor);
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

  const caors::serialization_text_result text = caors::serialize_text(actor);
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

  const caors::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value ==
        R"({"raw":7,"shared":8,"unique":9,"optional":10,"stats":{"level":2,"score":3.5}})");

  const caors::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::variant_member* shared = value.value.find_member(caors::string_name{"shared"});
  REQUIRE(shared != nullptr);
  CHECK(shared->value.as_i64() == 8);

  const caors::variant_member* nested = value.value.find_member(caors::string_name{"stats"});
  REQUIRE(nested != nullptr);
  REQUIRE(nested->value.is_object());
  const caors::variant_member* level = nested->value.find_member(caors::string_name{"level"});
  REQUIRE(level != nullptr);
  CHECK(level->value.as_i64() == 2);
}

TEST_CASE("serialization - deserializes pointer and wrapper fields into existing values",
          "[serialization]") {
  require_pointer_actor_metadata();

  caors::variant::object_type stats_members;
  stats_members.push_back(caors::variant_member{
      caors::string_name{"level"},
      0,
      caors::variant::from(std::int64_t{4}),
  });
  stats_members.push_back(caors::variant_member{
      caors::string_name{"score"},
      0,
      caors::variant::from(5.5),
  });

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"raw"},
      0,
      caors::variant::from(std::int64_t{70}),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"shared"},
      0,
      caors::variant::from(std::int64_t{80}),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"unique"},
      0,
      caors::variant::from(std::int64_t{90}),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"optional"},
      0,
      caors::variant::from(std::int64_t{100}),
  });
  members.push_back(caors::variant_member{
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
  REQUIRE(
      serialization_result_ok(caors::detail::register_serialization<reference_wrapper_actor>()));

  reference_wrapper_actor actor{7};

  const caors::serialization_text_result text = caors::serialize_text(actor);
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
  REQUIRE(
      serialization_result_ok(caors::detail::register_serialization<reference_wrapper_actor>()));

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

  const caors::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"raw":null,"shared":null,"unique":null,"optional":null,"stats":null})");

  const caors::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());

  const caors::variant_member* raw = value.value.find_member(caors::string_name{"raw"});
  const caors::variant_member* shared = value.value.find_member(caors::string_name{"shared"});
  const caors::variant_member* unique = value.value.find_member(caors::string_name{"unique"});
  const caors::variant_member* optional = value.value.find_member(caors::string_name{"optional"});
  const caors::variant_member* stats = value.value.find_member(caors::string_name{"stats"});
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

TEST_CASE("serialization - serializes vector fields as arrays", "[serialization]") {
  require_serialization_codec<caors::vector<int>>("caors.vector<int>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<vector_actor>("caors.test.serialization.vector_actor")
          .field("scores", &vector_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<vector_actor>()));

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
  require_serialization_codec<caors::vector<int>>("caors.vector<int>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<vector_actor>("caors.test.serialization.vector_actor")
          .field("scores", &vector_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<vector_actor>()));

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

TEST_CASE("serialization - recursively serializes nested vector values", "[serialization]") {
  require_serialization_codec<caors::vector<int>>("caors.vector<int>");
  require_registered_type<caors::vector<caors::vector<int>>>("caors.vector<caors.vector<int>>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<nested_vector_values_actor>(
          "caors.test.serialization.nested_vector_values_actor")
          .field("groups", &nested_vector_values_actor::groups, "caors.vector<caors.vector<int>>")
          .commit()));
  REQUIRE(
      serialization_result_ok(caors::detail::register_serialization<nested_vector_values_actor>()));

  nested_vector_values_actor actor{};
  actor.groups.push_back(caors::vector<int>{1, 2});
  actor.groups.push_back(caors::vector<int>{3});

  const caors::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"groups":[[1,2],[3]]})");

  const caors::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::variant_member* groups = value.value.find_member(caors::string_name{"groups"});
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

  const caors::serialization_text_result text = caors::serialize_json(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"values":[7,null,9]})");

  const caors::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::variant_member* values = value.value.find_member(caors::string_name{"values"});
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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<guid_vector_actor>()));

  const std::optional<caors::guid> first =
      caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  const std::optional<caors::guid> second =
      caors::guid::parse("5ed39a97-7b20-4d87-86ac-e62a4a615b10");
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());

  guid_vector_actor actor{};
  actor.ids.push_back(*first);
  actor.ids.push_back(*second);

  const caors::serialization_text_result text = caors::serialize_text(actor);
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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<list_actor>()));

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
  require_serialization_codec<caors::list<int>>("caors.list<int>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<list_actor>("caors.test.serialization.list_actor")
          .field("scores", &list_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<list_actor>()));

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

TEST_CASE("serialization - roundtrips std forward list fields", "[serialization]") {
  using scores_type = std::forward_list<int>;
  require_serialization_codec<scores_type>("std.forward_list<int>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<forward_list_actor>("caors.test.serialization.forward_list_actor")
          .field("scores", &forward_list_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<forward_list_actor>()));

  forward_list_actor actor{};
  actor.scores.assign({1, 2, 3});

  const caors::serialization_text_result text = caors::serialize_text(actor);
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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<deque_actor>()));

  deque_actor actor{};
  actor.scores.push_back(1);
  actor.scores.push_back(2);
  actor.scores.push_back(3);

  const caors::serialization_text_result text = caors::serialize_text(actor);
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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<array_actor>()));

  array_actor actor{{1, 2, 3}};

  const caors::serialization_text_result text = caors::serialize_text(actor);
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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<array_actor>()));

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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<span_actor>()));

  std::array<int, 3> storage{1, 2, 3};
  span_actor actor{std::span<int>{storage}};

  const caors::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"scores":[1,2,3]})");
}

TEST_CASE("serialization - rejects std span deserialization by default", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<span_actor>("caors.test.serialization.span_actor")
          .field("scores", &span_actor::scores)
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<span_actor>()));

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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<span_actor>()));

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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<pair_actor>()));

  pair_actor actor{{7, caors::string{"seven"}}};

  const caors::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"entry":[7,"seven"]})");

  pair_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"entry":[9,"nine"]})");

  REQUIRE(result.succeeded());
  CHECK(decoded.entry.first == 9);
  CHECK(decoded.entry.second == "nine");

  const caors::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::variant_member* entry = value.value.find_member(caors::string_name{"entry"});
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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<pair_actor>()));

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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<tuple_actor>()));

  tuple_actor actor{{7, caors::string{"seven"}, 1.5F}};

  const caors::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"entry":[7,"seven",1.5]})");

  tuple_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"entry":[9,"nine",2.5]})");

  REQUIRE(result.succeeded());
  CHECK(std::get<0>(decoded.entry) == 9);
  CHECK(std::get<1>(decoded.entry) == "nine");
  CHECK(std::get<2>(decoded.entry) == 2.5F);

  const caors::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::variant_member* entry = value.value.find_member(caors::string_name{"entry"});
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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<tuple_actor>()));

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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<variant_actor>()));

  variant_actor actor{};
  actor.entry = caors::string{"seven"};

  const caors::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"entry":[1,"seven"]})");

  variant_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"entry":[2,2.5]})");

  REQUIRE(result.succeeded());
  REQUIRE(std::holds_alternative<float>(decoded.entry));
  CHECK(std::get<float>(decoded.entry) == 2.5F);

  const caors::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::variant_member* entry = value.value.find_member(caors::string_name{"entry"});
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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<variant_actor>()));

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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<map_actor>()));

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
  require_serialization_codec<labels_type>("caors.hash_map<int,caors.string>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<map_actor>("caors.test.serialization.map_actor")
          .field("labels", &map_actor::labels)
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<map_actor>()));

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
  require_serialization_codec<labels_type>("caors.hash_map<int,caors.string>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<map_actor>("caors.test.serialization.map_actor")
          .field("labels", &map_actor::labels)
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<map_actor>()));

  const caors::variant_result parsed = caors::parse_json(R"({"labels":[[3,"three"]]})");
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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<ordered_map_actor>()));

  ordered_map_actor actor{};
  actor.labels.emplace(2, caors::string{"two"});
  actor.labels.emplace(1, caors::string{"one"});

  const caors::serialization_text_result text = caors::serialize_text(actor);
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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<set_actor>()));

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
  require_serialization_codec<ids_type>("caors.hash_set<int>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<set_actor>("caors.test.serialization.set_actor")
          .field("ids", &set_actor::ids)
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<set_actor>()));

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
  require_serialization_codec<ids_type>("caors.hash_set<int>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<set_actor>("caors.test.serialization.set_actor")
          .field("ids", &set_actor::ids)
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<set_actor>()));

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

TEST_CASE("serialization - roundtrips std set fields", "[serialization]") {
  using ids_type = std::set<int>;
  require_serialization_codec<ids_type>("std.set<int>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<ordered_set_actor>("caors.test.serialization.ordered_set_actor")
          .field("ids", &ordered_set_actor::ids)
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<ordered_set_actor>()));

  ordered_set_actor actor{};
  actor.ids.emplace(3);
  actor.ids.emplace(1);

  const caors::serialization_text_result text = caors::serialize_text(actor);
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
  REQUIRE(serialization_result_ok(
      caors::detail::register_serialization<custom_codec_sequence_actor>()));

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
  require_serialization_codec<labels_type>("caors.test.serialization.custom_map<int,caors.string>");
  REQUIRE(reflection_result_ok(caors::reflection::class_<custom_codec_map_actor>(
                                   "caors.test.serialization.custom_codec_map_actor")
                                   .field("labels", &custom_codec_map_actor::labels)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<custom_codec_map_actor>()));

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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<nested_stats>()));
  require_serialization_codec<caors::vector<nested_stats>>(
      "caors.vector<caors.test.serialization.nested_stats>");
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<nested_vector_actor>("caors.test.serialization.nested_vector_actor")
          .field("stats", &nested_vector_actor::stats)
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<nested_vector_actor>()));

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
  REQUIRE(type_result_ok(caors::detail::register_type<caors::object_ref<object_ref_target>>(
      "caors.object_ref<caors.test.serialization.object_ref_target>")));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ref_actor>("caors.test.serialization.object_ref_actor")
          .field("target", &object_ref_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<object_ref_actor>()));

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
  REQUIRE(type_result_ok(caors::detail::register_type<caors::object_ref<object_ref_target>>(
      "caors.object_ref<caors.test.serialization.object_ref_target>")));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ref_actor>("caors.test.serialization.object_ref_actor")
          .field("target", &object_ref_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<object_ref_actor>()));

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

TEST_CASE("serialization - serializes object references inside vectors", "[serialization]") {
  require_object_ref_vector_actor_metadata();

  object_ref_target target;
  object_ref_resolver_data resolver_data{caors::object_id{77}, &target};
  const caors::serialization_reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };
  const caors::serialization_context context{0, &resolver};

  object_ref_vector_actor actor{};
  actor.targets.push_back(caors::object_ref<object_ref_target>{&target});
  actor.targets.push_back(caors::object_ref<object_ref_target>{});

  const caors::serialization_text_result text = caors::serialize_json(actor, context);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"targets":[77,0]})");

  const caors::variant_result value = caors::to_serialization_value(actor, context);
  REQUIRE(value.succeeded());
  const caors::variant_member* targets = value.value.find_member(caors::string_name{"targets"});
  REQUIRE(targets != nullptr);
  REQUIRE(targets->value.is_array());
  REQUIRE(targets->value.as_array().size() == 2);
  CHECK(targets->value.as_array()[0].as_u64() == 77);
  CHECK(targets->value.as_array()[1].as_u64() == 0);
}

TEST_CASE("serialization - deserializes object references inside vectors", "[serialization]") {
  require_object_ref_vector_actor_metadata();

  object_ref_target target;
  object_ref_resolver_data resolver_data{caors::object_id{88}, &target};
  const caors::serialization_reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };

  object_ref_vector_actor actor{};
  const caors::error result = caors::deserialize_json(actor, R"({"targets":[88,0]})",
                                                      caors::serialization_context{0, &resolver});

  REQUIRE(result.succeeded());
  REQUIRE(actor.targets.size() == 2);
  CHECK(actor.targets[0].get() == &target);
  CHECK(actor.targets[1].get() == nullptr);
}

TEST_CASE("serialization - object reference resolver failures are reported", "[serialization]") {
  REQUIRE(type_result_ok(caors::detail::register_type<caors::object_ref<object_ref_target>>(
      "caors.object_ref<caors.test.serialization.object_ref_target>")));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ref_actor>("caors.test.serialization.object_ref_actor")
          .field("target", &object_ref_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<object_ref_actor>()));

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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<reader_field_id_actor>({
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
  REQUIRE(
      serialization_result_ok(caors::detail::register_serialization<reader_missing_field_actor>()));

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

  REQUIRE(serialization_result_ok(caors::detail::register_serialization<reader_field_policy_actor>({
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

  REQUIRE(serialization_result_ok(caors::detail::register_serialization<reader_field_policy_actor>({
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
  REQUIRE(
      serialization_result_ok(caors::detail::register_serialization<reader_unknown_field_actor>({
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
  future.required = true;

  REQUIRE(serialization_result_ok(caors::detail::register_serialization<versioned_actor>({
      id,
      legacy,
      current,
      future,
  })));

  const versioned_actor actor{1, 10, 20, 30};
  const caors::serialization_text_result text_v2 =
      caors::serialize_text(actor, caors::serialization_context{2});

  REQUIRE(text_v2.succeeded());
  CHECK(text_v2.value == R"({"id":1,"current":20})");

  const caors::serialization_text_result text_v3 =
      caors::serialize_text(actor, caors::serialization_context{3});

  REQUIRE(text_v3.succeeded());
  CHECK(text_v3.value == R"({"id":1,"current":20,"future":30})");

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

  caors::variant::object_type extra_members;
  extra_members.push_back(caors::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{3}),
  });
  extra_members.push_back(caors::variant_member{
      caors::string_name{"legacy"},
      0,
      caors::variant::from(std::int64_t{33}),
  });
  extra_members.push_back(caors::variant_member{
      caors::string_name{"current"},
      0,
      caors::variant::from(std::int64_t{44}),
  });
  extra_members.push_back(caors::variant_member{
      caors::string_name{"future"},
      0,
      caors::variant::from(std::int64_t{55}),
  });

  versioned_actor filtered{0, 10, 0, 30};
  const caors::error filtered_result = caors::deserialize(
      filtered, caors::variant::object(std::move(extra_members)), caors::serialization_context{2});

  REQUIRE(filtered_result.succeeded());
  CHECK(filtered.id == 3);
  CHECK(filtered.legacy == 10);
  CHECK(filtered.current == 44);
  CHECK(filtered.future == 30);

  caors::variant::object_type missing_required_members;
  missing_required_members.push_back(caors::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{4}),
  });
  missing_required_members.push_back(caors::variant_member{
      caors::string_name{"current"},
      0,
      caors::variant::from(std::int64_t{55}),
  });

  versioned_actor missing_required{};
  const caors::error missing_required_result = caors::deserialize(
      missing_required, caors::variant::object(std::move(missing_required_members)),
      caors::serialization_context{3});

  CHECK(missing_required_result.failed());
  CHECK(missing_required_result.domain() == caors::error_domain::serialization);
  CHECK(missing_required_result.value() == caors::errc::not_found);
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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<codec_guid_actor>()));

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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<codec_guid_actor>()));

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
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<codec_guid_actor>()));

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
  REQUIRE(
      serialization_result_ok(caors::detail::register_serialization<codec_string_view_actor>()));

  const codec_string_view_actor actor{std::string_view{"Ada"}};
  const caors::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"name":"Ada"})");
}

TEST_CASE("serialization - built in codecs reject string view deserialization", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<codec_string_view_actor>(
                                   "caors.test.serialization.codec_string_view_actor")
                                   .field("name", &codec_string_view_actor::name)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::detail::register_serialization<codec_string_view_actor>()));

  codec_string_view_actor actor{std::string_view{"original"}};
  const caors::error result = caors::deserialize_json(actor, R"({"name":"Ada"})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
  CHECK(actor.name == "original");
}

TEST_CASE("serialization - string view deserialization can use explicit backing storage",
          "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<codec_string_view_actor>(
                                   "caors.test.serialization.codec_string_view_actor")
                                   .field("name", &codec_string_view_actor::name)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::detail::register_serialization<codec_string_view_actor>()));

  codec_string_view_actor actor{std::string_view{"original"}};
  caors::string backing;
  caors::serialization_context context{};
  context.string_view_storage = &backing;

  const caors::error result = caors::deserialize_json(actor, R"({"name":"Ada"})", context);

  REQUIRE(result.succeeded());
  CHECK(backing == "Ada");
  CHECK(actor.name == "Ada");
  CHECK(actor.name.data() == backing.data());
}

TEST_CASE("serialization - built in codecs roundtrip filesystem paths as strings",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<codec_path_actor>("caors.test.serialization.codec_path_actor")
          .field("path", &codec_path_actor::path)
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<codec_path_actor>()));

  codec_path_actor actor{std::filesystem::path{"assets/textures/albedo.png"}};

  const caors::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"path":"assets/textures/albedo.png"})");

  const caors::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::variant_member* path = value.value.find_member(caors::string_name{"path"});
  REQUIRE(path != nullptr);
  REQUIRE(path->value.try_as_string() != nullptr);
  CHECK(*path->value.try_as_string() == "assets/textures/albedo.png");

  codec_path_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"path":"levels/start.scene"})");

  REQUIRE(result.succeeded());
  CHECK(decoded.path.generic_string() == "levels/start.scene");
}

TEST_CASE("serialization - built in codecs roundtrip chrono values as integers",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<codec_chrono_actor>("caors.test.serialization.codec_chrono_actor")
          .field("timeout", &codec_chrono_actor::timeout)
          .field("timestamp", &codec_chrono_actor::timestamp)
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<codec_chrono_actor>()));

  codec_chrono_actor actor{
      std::chrono::milliseconds{2500},
      std::chrono::system_clock::time_point{std::chrono::milliseconds{1700000000123}},
  };

  const caors::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"timeout":2500,"timestamp":1700000000123})");

  const caors::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::variant_member* timeout = value.value.find_member(caors::string_name{"timeout"});
  const caors::variant_member* timestamp = value.value.find_member(caors::string_name{"timestamp"});
  REQUIRE(timeout != nullptr);
  REQUIRE(timestamp != nullptr);
  REQUIRE(timeout->value.try_as_i64() != nullptr);
  REQUIRE(timestamp->value.try_as_i64() != nullptr);
  CHECK(*timeout->value.try_as_i64() == 2500);
  CHECK(*timestamp->value.try_as_i64() == 1700000000123);

  codec_chrono_actor decoded{};
  const caors::error result =
      caors::deserialize_json(decoded, R"({"timeout":125,"timestamp":1700000000456})");

  REQUIRE(result.succeeded());
  CHECK(decoded.timeout == std::chrono::milliseconds{125});
  CHECK(decoded.timestamp.time_since_epoch() == std::chrono::milliseconds{1700000000456});
}

TEST_CASE("serialization - writer features validate required field identifiers",
          "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<numeric_writer_actor>(
                                   "caors.test.serialization.numeric_writer_actor")
                                   .field("id", &numeric_writer_actor::id)
                                   .commit()));
  REQUIRE(caors::detail::register_serialization<numeric_writer_actor>(
              {
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
  const caors::meta_type type =
      caors::meta<serialization_first_actor>("caors.test.serialization.serialization_first_actor")
          .get();
  REQUIRE(type.valid());

  REQUIRE(caors::detail::register_serialization<serialization_first_actor>(
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
  REQUIRE(caors::detail::register_serialization<missing_field_actor>(
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
  REQUIRE(
      serialization_result_ok(caors::detail::register_serialization<unsupported_field_actor>()));

  const unsupported_field_actor actor{{1}};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::unsupported);
}

TEST_CASE("serialization - reports unsupported weak pointer fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<weak_ptr_actor>("caors.test.serialization.weak_ptr_actor")
          .field("value", &weak_ptr_actor::value, "std.weak_ptr<int>")
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<weak_ptr_actor>()));

  const std::shared_ptr<int> value = std::make_shared<int>(7);
  const weak_ptr_actor actor{value};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::unsupported);
}

TEST_CASE("serialization - rejects weak pointer deserialization", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<weak_ptr_actor>("caors.test.serialization.weak_ptr_actor")
          .field("value", &weak_ptr_actor::value, "std.weak_ptr<int>")
          .commit()));
  REQUIRE(serialization_result_ok(caors::detail::register_serialization<weak_ptr_actor>()));

  weak_ptr_actor actor{};
  const caors::error result = caors::deserialize_json(actor, R"({"value":7})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
  CHECK(actor.value.expired());
}
