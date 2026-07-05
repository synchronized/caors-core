// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/reflection.h"
#include "caors/serialization.h"
#include "caors/traits/type_name.h"

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

namespace caors::tests::serialization {

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

struct codec_string_view_pair_actor {
  std::string_view first{};
  std::string_view second{};
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
  caors::tests::serialization::custom_codec_sequence<int> scores{};
};

template <typename K, typename V> struct custom_codec_map {
  caors::hash_map<K, V> values{};
};

struct custom_codec_map_actor {
  caors::tests::serialization::custom_codec_map<int, caors::string> labels{};
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

[[nodiscard]] inline bool reflection_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_reflection;
}

[[nodiscard]] inline bool serialization_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_component;
}

[[nodiscard]] inline bool type_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_id;
}

template <typename T> void require_registered_type(std::string_view name) {
  REQUIRE(type_result_ok(caors::meta::type_<T>(name).result()));
}

template <typename T> void register_common_type(std::string_view name) {
  const caors::error status = caors::meta::type_<T>(name).result();
  (void)status;
}

template <typename T> void require_serialization_codec(std::string_view name) {
  require_registered_type<T>(name);
  REQUIRE(serialization_result_ok(caors::serialization::register_codec<T>()));
}

inline void require_pointer_actor_metadata() {
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
  REQUIRE(serialization_result_ok(caors::serialization::register_type<nested_stats>()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<pointer_actor>()));
}

inline void require_non_default_value_metadata() {
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
  REQUIRE(serialization_result_ok(caors::serialization::register_type<non_default_value>()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<non_default_field_actor>()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<non_default_vector_actor>()));
  REQUIRE(
      serialization_result_ok(caors::serialization::register_type<non_default_unique_ptr_actor>()));
  REQUIRE(
      serialization_result_ok(caors::serialization::register_type<non_default_variant_actor>()));
}

inline void require_unique_ptr_vector_actor_metadata() {
  require_registered_type<std::unique_ptr<int>>("std.unique_ptr<int>");
  REQUIRE(reflection_result_ok(caors::reflection::class_<unique_ptr_vector_actor>(
                                   "caors.test.serialization.unique_ptr_vector_actor")
                                   .field("values", &unique_ptr_vector_actor::values)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<unique_ptr_vector_actor>()));
}

inline void require_scalar_actor_metadata() {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<scalar_actor>("caors.test.serialization.scalar_actor")
          .field("code", &scalar_actor::code)
          .field("color", &scalar_actor::color, "caors.test.serialization.scalar_color")
          .field("state", &scalar_actor::state, "caors.test.serialization.scalar_state")
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<scalar_actor>()));
}

inline void require_object_ref_vector_actor_metadata() {
  require_registered_type<caors::object_ref<object_ref_target>>(
      "caors.object_ref<caors.test.serialization.object_ref_target>");
  REQUIRE(reflection_result_ok(caors::reflection::class_<object_ref_vector_actor>(
                                   "caors.test.serialization.object_ref_vector_actor")
                                   .field("targets", &object_ref_vector_actor::targets)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<object_ref_vector_actor>()));
}

[[nodiscard]] inline caors::object_id resolve_object_id(const caors::object* instance,
                                                        void* user_data) {
  const auto* data = static_cast<const object_ref_resolver_data*>(user_data);
  if (data == nullptr || instance == nullptr || instance != data->instance) {
    return caors::object_id::invalid();
  }

  return data->id;
}

[[nodiscard]] inline caors::object* resolve_object(caors::object_id id, void* user_data) {
  const auto* data = static_cast<const object_ref_resolver_data*>(user_data);
  if (data == nullptr || id != data->id) {
    return nullptr;
  }

  return data->instance;
}

class numeric_field_writer final : public caors::serialization::writer {
public:
  [[nodiscard]] caors::serialization::format_features features() const noexcept override {
    return caors::serialization::format_features::numeric_fields;
  }

  [[nodiscard]] caors::error begin_object(const caors::serialization::type_data&) override {
    object_open = true;
    return {};
  }

  [[nodiscard]] caors::error begin_field(const caors::serialization::field_data& field) override {
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

} // namespace caors::tests::serialization

namespace caors::traits {

template <> struct type_name<caors::tests::serialization::unsupported_field_value> {
  static constexpr std::string_view value = "caors.test.serialization.unsupported_value";
};

template <> struct type_name<caors::tests::serialization::nested_stats> {
  static constexpr std::string_view value = "caors.test.serialization.nested_stats";
};

} // namespace caors::traits

namespace caors::traits {

template <typename T>
struct sequence_traits<caors::tests::serialization::custom_codec_sequence<T>> {
  static constexpr bool is_sequence = true;
  using element_type = std::remove_cv_t<T>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    return "caors.test.serialization.custom_sequence";
  }

  [[nodiscard]] static std::size_t
  size(const caors::tests::serialization::custom_codec_sequence<T>& values) noexcept {
    return values.values.size();
  }

  [[nodiscard]] static const element_type*
  element(const caors::tests::serialization::custom_codec_sequence<T>& values,
          std::size_t index) noexcept {
    return index < values.values.size() ? &values.values[index] : nullptr;
  }

  [[nodiscard]] static element_type*
  mutable_element(caors::tests::serialization::custom_codec_sequence<T>& values,
                  std::size_t index) noexcept {
    return index < values.values.size() ? &values.values[index] : nullptr;
  }

  static void clear(caors::tests::serialization::custom_codec_sequence<T>& values) {
    values.values.clear();
  }

  [[nodiscard]] static bool resize(caors::tests::serialization::custom_codec_sequence<T>& values,
                                   std::size_t size) {
    values.values.resize(size);
    return true;
  }

  [[nodiscard]] static bool append(caors::tests::serialization::custom_codec_sequence<T>& values,
                                   const element_type& value) {
    values.values.push_back(value);
    return true;
  }
};

template <typename K, typename V>
struct associative_traits<caors::tests::serialization::custom_codec_map<K, V>> {
  static constexpr bool is_associative = true;
  using key_type = std::remove_cv_t<K>;
  using mapped_type = std::remove_cv_t<V>;
  using map_type = caors::tests::serialization::custom_codec_map<K, V>;

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

} // namespace caors::traits

namespace caors::serialization {

template <typename T>
struct codec<caors::tests::serialization::custom_codec_sequence<T>>
    : sequence_codec<caors::tests::serialization::custom_codec_sequence<T>> {};

template <typename K, typename V>
struct codec<caors::tests::serialization::custom_codec_map<K, V>>
    : associative_codec<caors::tests::serialization::custom_codec_map<K, V>> {};

} // namespace caors::serialization

namespace caors::tests::serialization {

[[maybe_unused]] inline const bool common_serialization_types_registered = [] {
  register_common_type<bool>("bool");
  register_common_type<char>("char");
  register_common_type<int>("int");
  register_common_type<float>("float");
  register_common_type<caors::string>("caors.string");
  register_common_type<std::string>("std.string");
  register_common_type<std::string_view>("std.string_view");
  register_common_type<caors::guid>("caors.guid");
  register_common_type<caors::type_guid>("caors.type_guid");
  register_common_type<std::filesystem::path>("std.filesystem.path");
  register_common_type<std::chrono::milliseconds>("std.chrono.milliseconds");
  register_common_type<std::chrono::system_clock::time_point>("std.chrono.system_clock.time_point");
  register_common_type<scalar_color>("caors.test.serialization.scalar_color");
  register_common_type<scalar_state>("caors.test.serialization.scalar_state");
  register_common_type<unsupported_field_value>("caors.test.serialization.unsupported_value");
  register_common_type<nested_stats>("caors.test.serialization.nested_stats");
  register_common_type<non_default_value>("caors.test.serialization.non_default_value");
  register_common_type<int*>("int*");
  register_common_type<nested_stats*>("caors.test.serialization.nested_stats*");
  register_common_type<std::shared_ptr<int>>("std.shared_ptr<int>");
  register_common_type<std::unique_ptr<int>>("std.unique_ptr<int>");
  register_common_type<std::weak_ptr<int>>("std.weak_ptr<int>");
  register_common_type<std::optional<int>>("std.optional<int>");
  register_common_type<std::reference_wrapper<int>>("std.reference_wrapper<int>");
  register_common_type<std::unique_ptr<non_default_value>>(
      "std.unique_ptr<caors.test.serialization.non_default_value>");
  register_common_type<caors::vector<int>>("caors.vector<int>");
  register_common_type<caors::vector<caors::vector<int>>>("caors.vector<caors.vector<int>>");
  register_common_type<caors::vector<std::unique_ptr<int>>>("caors.vector<std.unique_ptr<int>>");
  register_common_type<caors::vector<caors::guid>>("caors.vector<guid>");
  register_common_type<caors::vector<non_default_value>>(
      "caors.vector<caors.test.serialization.non_default_value>");
  register_common_type<caors::vector<nested_stats>>(
      "caors.vector<caors.test.serialization.nested_stats>");
  register_common_type<caors::vector<caors::object_ref<object_ref_target>>>(
      "caors.vector<caors.object_ref<caors.test.serialization.object_ref_target>>");
  register_common_type<caors::list<int>>("caors.list<int>");
  register_common_type<std::forward_list<int>>("std.forward_list<int>");
  register_common_type<std::deque<int>>("std.deque<int>");
  register_common_type<std::array<int, 3>>("std.array<int,3>");
  register_common_type<std::span<int>>("std.span<int>");
  register_common_type<std::pair<int, caors::string>>("std.pair<int,caors.string>");
  register_common_type<std::tuple<int, caors::string, float>>("std.tuple<int,caors.string,float>");
  register_common_type<std::variant<int, caors::string, float>>(
      "std.variant<int,caors.string,float>");
  register_common_type<std::variant<int, non_default_value>>(
      "std.variant<int,caors.test.serialization.non_default_value>");
  register_common_type<caors::hash_map<int, caors::string>>("caors.hash_map<int,caors.string>");
  register_common_type<std::map<int, caors::string>>("std.map<int,caors.string>");
  register_common_type<caors::hash_set<int>>("caors.hash_set<int>");
  register_common_type<std::set<int>>("std.set<int>");
  register_common_type<custom_codec_sequence<int>>("caors.test.serialization.custom_sequence<int>");
  register_common_type<custom_codec_map<int, caors::string>>(
      "caors.test.serialization.custom_map<int,caors.string>");
  register_common_type<caors::object_ref<object_ref_target>>(
      "caors.object_ref<caors.test.serialization.object_ref_target>");
  return true;
}();

} // namespace caors::tests::serialization
