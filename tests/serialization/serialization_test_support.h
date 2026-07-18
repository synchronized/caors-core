// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/meta.h"
#include "caors/registration.h"
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

struct non_default_optional_actor {
  std::optional<non_default_value> value{};
};

struct non_default_variant_actor {
  std::variant<int, non_default_value> value{0};
};

struct takeover_rollback_value {
  explicit takeover_rollback_value(int input) : value(input) { ++constructed_count; }
  ~takeover_rollback_value() { ++destroyed_count; }

  static void reset_counts() noexcept {
    constructed_count = 0;
    destroyed_count = 0;
  }

  inline static int constructed_count = 0;
  inline static int destroyed_count = 0;

  int value = 0;
};

struct failing_takeover_wrapper {
  takeover_rollback_value* value = nullptr;
};

struct failing_takeover_actor {
  failing_takeover_wrapper value{};
};

struct pointer_actor {
  int* raw = nullptr;
  std::shared_ptr<int> shared{};
  std::unique_ptr<int> unique{};
  std::optional<int> optional{};
  nested_stats* stats = nullptr;
};

struct recursive_node {
  int value = 0;
  recursive_node* next = nullptr;
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

template <typename T> struct custom_sequence {
  caors::vector<T> values{};
};

struct custom_sequence_actor {
  caors::tests::serialization::custom_sequence<int> scores{};
};

template <typename K, typename V> struct custom_map {
  caors::hash_map<K, V> values{};
};

struct custom_map_actor {
  caors::tests::serialization::custom_map<int, caors::string> labels{};
};

struct nested_vector_actor {
  caors::vector<nested_stats> stats{};
};

struct object_ptr_target : caors::object {};

struct other_object_ptr_target : caors::object {};

struct object_ptr_actor {
  caors::object_ptr<object_ptr_target> target{};
};

struct object_ptr_vector_actor {
  caors::vector<caors::object_ptr<object_ptr_target>> targets{};
};

struct object_ptr_resolver_data {
  caors::object_id id{};
  caors::object* instance = nullptr;
};

[[nodiscard]] inline bool reflection_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_field ||
         result.value() == caors::errc::duplicate_method;
}

[[nodiscard]] inline bool serialization_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_component;
}

[[nodiscard]] inline bool type_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_id;
}

template <typename T> void require_registered_type(std::string_view name) {
  REQUIRE(type_result_ok(caors::meta::class_<T>(name).result()));
}

template <typename T>
[[nodiscard]] caors::meta::class_builder<T> registered_meta_class(std::string_view name) {
  REQUIRE(type_result_ok(caors::meta::class_<T>(name).result()));
  return caors::meta::class_<T>();
}

inline void require_pointer_actor_metadata() {
  REQUIRE(reflection_result_ok(
      registered_meta_class<nested_stats>("caors.test.serialization.nested_stats")
          .property("level", &nested_stats::level)
          .property("score", &nested_stats::score)
          .commit()));
  REQUIRE(reflection_result_ok(
      registered_meta_class<pointer_actor>("caors.test.serialization.pointer_actor")
          .property("raw", &pointer_actor::raw)
          .property("shared", &pointer_actor::shared)
          .property("unique", &pointer_actor::unique)
          .property("optional", &pointer_actor::optional)
          .property("stats", &pointer_actor::stats)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<nested_stats>().commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<pointer_actor>().commit()));
}

inline void require_recursive_node_metadata() {
  REQUIRE(reflection_result_ok(
      registered_meta_class<recursive_node>("caors.test.serialization.recursive_node")
          .property("value", &recursive_node::value)
          .property("next", &recursive_node::next)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<recursive_node>().commit()));
}

inline void require_reference_wrapper_actor_metadata() {
  REQUIRE(reflection_result_ok(registered_meta_class<reference_wrapper_actor>(
                                   "caors.test.serialization.reference_wrapper_actor")
                                   .property("value", &reference_wrapper_actor::value)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::class_<reference_wrapper_actor>().commit()));
}

inline void require_non_default_value_metadata() {
  REQUIRE(type_result_ok(
      caors::meta::class_<non_default_value>("caors.test.serialization.non_default_value")
          .constructor<int>()
          .result()));
  REQUIRE(reflection_result_ok(
      registered_meta_class<non_default_value>("caors.test.serialization.non_default_value")
          .property("value", &non_default_value::value)
          .commit()));
  REQUIRE(reflection_result_ok(registered_meta_class<non_default_field_actor>(
                                   "caors.test.serialization.non_default_field_actor")
                                   .property("value", &non_default_field_actor::value)
                                   .commit()));
  REQUIRE(reflection_result_ok(registered_meta_class<non_default_vector_actor>(
                                   "caors.test.serialization.non_default_vector_actor")
                                   .property("values", &non_default_vector_actor::values)
                                   .commit()));
  REQUIRE(reflection_result_ok(registered_meta_class<non_default_unique_ptr_actor>(
                                   "caors.test.serialization.non_default_unique_ptr_actor")
                                   .property("value", &non_default_unique_ptr_actor::value)
                                   .commit()));
  REQUIRE(reflection_result_ok(registered_meta_class<non_default_optional_actor>(
                                   "caors.test.serialization.non_default_optional_actor")
                                   .property("value", &non_default_optional_actor::value)
                                   .commit()));
  REQUIRE(reflection_result_ok(registered_meta_class<non_default_variant_actor>(
                                   "caors.test.serialization.non_default_variant_actor")
                                   .property("value", &non_default_variant_actor::value)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<non_default_value>().commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::class_<non_default_field_actor>().commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::class_<non_default_vector_actor>().commit()));
  REQUIRE(serialization_result_ok(
      caors::serialization::class_<non_default_unique_ptr_actor>().commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::class_<non_default_optional_actor>().commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::class_<non_default_variant_actor>().commit()));
}

inline caors::serialization::construction_result construct_non_default_value_for_serialization(
    const caors::serialization::construction_request& request,
    const caors::serialization::context& context, void*) {
  const caors::meta::type expected = caors::meta::resolve<non_default_value>();
  if (!request.type || request.type.id() != expected.id()) {
    return {};
  }

  caors::any initial = caors::any::from(0);
  std::array<caors::argument, 1> arguments{caors::arg(initial.ref())};
  const caors::meta::constructor_capability constructor =
      request.type.get_capability<caors::meta::constructor_capability>();
  caors::owned_value value = constructor.construct_owned(
      std::span<const caors::argument>{arguments.data(), arguments.size()},
      context.effective_memory_resource());
  caors::released_owned_value released = std::move(value).release();
  return {released.value, released.resource_destroy};
}

inline caors::serialization::context non_default_construction_context() {
  caors::serialization::context context{};
  context.resolvers.construction.construct = &construct_non_default_value_for_serialization;
  return context;
}

inline void destroy_takeover_rollback_value(caors::memory_resource*, void* value) noexcept {
  delete static_cast<takeover_rollback_value*>(value);
}

inline caors::serialization::construction_result
construct_takeover_rollback_value(const caors::serialization::construction_request& request,
                                  const caors::serialization::context&, void*) {
  const caors::meta::type expected = caors::meta::resolve<takeover_rollback_value>();
  if (!request.type || request.type.id() != expected.id()) {
    return {};
  }

  return {new takeover_rollback_value(0), &destroy_takeover_rollback_value};
}

inline caors::serialization::context takeover_rollback_construction_context() {
  caors::serialization::context context{};
  context.resolvers.construction.construct = &construct_takeover_rollback_value;
  return context;
}

inline void require_failing_takeover_actor_metadata() {
  REQUIRE(reflection_result_ok(registered_meta_class<takeover_rollback_value>(
                                   "caors.test.serialization.takeover_rollback_value")
                                   .property("value", &takeover_rollback_value::value)
                                   .commit()));
  REQUIRE(reflection_result_ok(registered_meta_class<failing_takeover_actor>(
                                   "caors.test.serialization.failing_takeover_actor")
                                   .property("value", &failing_takeover_actor::value)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::class_<takeover_rollback_value>().commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<failing_takeover_actor>().commit()));
}

inline void require_unique_ptr_vector_actor_metadata() {
  REQUIRE(reflection_result_ok(registered_meta_class<unique_ptr_vector_actor>(
                                   "caors.test.serialization.unique_ptr_vector_actor")
                                   .property("values", &unique_ptr_vector_actor::values)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::class_<unique_ptr_vector_actor>().commit()));
}

inline void require_scalar_actor_metadata() {
  REQUIRE(reflection_result_ok(
      registered_meta_class<scalar_actor>("caors.test.serialization.scalar_actor")
          .property("code", &scalar_actor::code)
          .property("color", &scalar_actor::color)
          .property("state", &scalar_actor::state)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<scalar_actor>().commit()));
}

inline void require_object_ptr_vector_actor_metadata() {
  REQUIRE(reflection_result_ok(registered_meta_class<object_ptr_vector_actor>(
                                   "caors.test.serialization.object_ptr_vector_actor")
                                   .property("targets", &object_ptr_vector_actor::targets)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::class_<object_ptr_vector_actor>().commit()));
}

[[nodiscard]] inline caors::object_id resolve_object_id(const caors::object* instance,
                                                        void* user_data) {
  const auto* data = static_cast<const object_ptr_resolver_data*>(user_data);
  if (data == nullptr || instance == nullptr || instance != data->instance) {
    return caors::object_id::invalid();
  }

  return data->id;
}

[[nodiscard]] inline caors::object* resolve_object(caors::object_id id, void* user_data) {
  const auto* data = static_cast<const object_ptr_resolver_data*>(user_data);
  if (data == nullptr || id != data->id) {
    return nullptr;
  }

  return data->instance;
}

[[nodiscard]] inline bool encode_object_ref(const void* instance, caors::type_id type,
                                            caors::serialization::external_ref& ref,
                                            void* user_data) {
  const auto* data = static_cast<const object_ptr_resolver_data*>(user_data);
  ref = caors::objects::make_object_external_ref(caors::object_id{}, type);
  if (instance == nullptr) {
    return true;
  }

  if (data == nullptr || instance != data->instance) {
    return false;
  }

  ref = caors::objects::make_object_external_ref(data->id, type);
  return true;
}

[[nodiscard]] inline void* resolve_object_ref(const caors::serialization::external_ref& ref,
                                              void* user_data) {
  const auto* data = static_cast<const object_ptr_resolver_data*>(user_data);
  if (data == nullptr || ref.domain != caors::objects::object_ref_domain() ||
      ref.identity != caors::serialization::external_ref_identity::integer ||
      caors::object_id{ref.integer_id} != data->id) {
    return nullptr;
  }

  return data->instance;
}

[[nodiscard]] inline caors::objects::object_ref_resolver_ops
make_test_object_ref_resolver(object_ptr_resolver_data& data) {
  return caors::objects::object_ref_resolver_ops{
      &resolve_object_ref,
      &data,
      caors::serialization::external_ref_identity::integer,
      &encode_object_ref,
  };
}

class numeric_field_writer final : public caors::serialization::writer {
public:
  [[nodiscard]] caors::serialization::format_features features() const noexcept override {
    return caors::serialization::format_features::numeric_fields;
  }

  [[nodiscard]] caors::error begin_object(const caors::serialization::type_schema&) override {
    object_open = true;
    return {};
  }

  [[nodiscard]] caors::error begin_field(const caors::serialization::field_schema& field) override {
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

template <> struct wrapper_traits<caors::tests::serialization::failing_takeover_wrapper> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = true;
  static constexpr wrapper_semantics semantics = wrapper_semantics::inline_value;
  using wrapped_type = caors::tests::serialization::takeover_rollback_value;
};

template <> struct wrapper_access<caors::tests::serialization::failing_takeover_wrapper> {
  using wrapper_type = caors::tests::serialization::failing_takeover_wrapper;
  using wrapped_type = caors::tests::serialization::takeover_rollback_value;

  [[nodiscard]] static const wrapped_type* value(const wrapper_type& wrapper) noexcept {
    return wrapper.value;
  }

  [[nodiscard]] static wrapped_type* mutable_value(wrapper_type& wrapper) noexcept {
    return wrapper.value;
  }

  [[nodiscard]] static bool has_value(const wrapper_type& wrapper) noexcept {
    return wrapper.value != nullptr;
  }

  static bool reset(wrapper_type& wrapper) noexcept {
    wrapper.value = nullptr;
    return true;
  }

  static bool ensure_value(wrapper_type&) { return false; }

  static bool set_value(wrapper_type&, wrapped_type*) noexcept { return false; }
};

template <typename T> struct sequence_traits<caors::tests::serialization::custom_sequence<T>> {
  static constexpr bool is_sequence = true;
  using element_type = std::remove_cv_t<T>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    return "caors.test.serialization.custom_sequence";
  }

  [[nodiscard]] static std::size_t
  size(const caors::tests::serialization::custom_sequence<T>& values) noexcept {
    return values.values.size();
  }

  [[nodiscard]] static const element_type*
  element(const caors::tests::serialization::custom_sequence<T>& values,
          std::size_t index) noexcept {
    return index < values.values.size() ? &values.values[index] : nullptr;
  }

  [[nodiscard]] static element_type*
  mutable_element(caors::tests::serialization::custom_sequence<T>& values,
                  std::size_t index) noexcept {
    return index < values.values.size() ? &values.values[index] : nullptr;
  }

  static void clear(caors::tests::serialization::custom_sequence<T>& values) {
    values.values.clear();
  }

  [[nodiscard]] static bool resize(caors::tests::serialization::custom_sequence<T>& values,
                                   std::size_t size) {
    values.values.resize(size);
    return true;
  }

  [[nodiscard]] static bool append(caors::tests::serialization::custom_sequence<T>& values,
                                   const element_type& value) {
    values.values.push_back(value);
    return true;
  }
};

template <typename K, typename V>
struct associative_traits<caors::tests::serialization::custom_map<K, V>> {
  static constexpr bool is_associative = true;
  using key_type = std::remove_cv_t<K>;
  using mapped_type = std::remove_cv_t<V>;
  using map_type = caors::tests::serialization::custom_map<K, V>;
  using const_iterator = typename caors::hash_map<K, V>::const_iterator;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    return "caors.test.serialization.custom_map";
  }

  [[nodiscard]] static std::size_t size(const map_type& values) noexcept {
    return values.values.size();
  }

  [[nodiscard]] static const_iterator begin(const map_type& values) noexcept {
    return values.values.begin();
  }

  [[nodiscard]] static const_iterator end(const map_type& values) noexcept {
    return values.values.end();
  }

  [[nodiscard]] static const key_type* key(const const_iterator& iterator) noexcept {
    return &iterator->first;
  }

  [[nodiscard]] static const mapped_type* mapped(const const_iterator& iterator) noexcept {
    return &iterator->second;
  }

  static void clear(map_type& values) { values.values.clear(); }

  [[nodiscard]] static bool insert(map_type& values, key_type key, mapped_type value) {
    return values.values.emplace(std::move(key), std::move(value)).second;
  }
};

} // namespace caors::traits

namespace caors::tests::serialization {} // namespace caors::tests::serialization
