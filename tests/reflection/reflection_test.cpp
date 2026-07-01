// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/adapters/stl.h"
#include "caors/reflection.h"

#include <catch2/catch_all.hpp>

#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>

namespace test_reflection {

template <typename T> struct custom_inline_wrapper {
  T value{};
};

template <typename T> struct custom_object_ref {
  T* value = nullptr;
};

struct named_field_value {
  int value = 0;
};

struct named_field_owner {
  named_field_value value{};
};

template <typename T> struct custom_sequence {
  caors::vector<T> values{};
};

} // namespace test_reflection

namespace caors {

template <typename T> struct wrapper_traits<test_reflection::custom_inline_wrapper<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = false;
  static constexpr wrapper_semantics semantics = wrapper_semantics::inline_value;
  using wrapped_type = T;
};

template <typename T> struct wrapper_traits<test_reflection::custom_object_ref<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = true;
  static constexpr wrapper_semantics semantics = wrapper_semantics::object_ref;
  using wrapped_type = T;
};

template <> struct type_name<test_reflection::named_field_value> {
  static constexpr std::string_view value = "caors.test.reflection.named_field_value";
};

template <typename T> struct sequence_traits<test_reflection::custom_sequence<T>> {
  static constexpr bool is_sequence = true;
  using element_type = std::remove_cv_t<T>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    return "test.custom_sequence";
  }

  [[nodiscard]] static std::size_t
  size(const test_reflection::custom_sequence<T>& values) noexcept {
    return values.values.size();
  }

  [[nodiscard]] static const element_type*
  element(const test_reflection::custom_sequence<T>& values, std::size_t index) noexcept {
    return index < values.values.size() ? &values.values[index] : nullptr;
  }

  [[nodiscard]] static element_type* mutable_element(test_reflection::custom_sequence<T>& values,
                                                     std::size_t index) noexcept {
    return index < values.values.size() ? &values.values[index] : nullptr;
  }

  static void clear(test_reflection::custom_sequence<T>& values) { values.values.clear(); }

  [[nodiscard]] static bool resize(test_reflection::custom_sequence<T>& values, std::size_t size) {
    if constexpr (std::is_default_constructible_v<T>) {
      values.values.resize(size);
      return true;
    } else {
      return false;
    }
  }

  [[nodiscard]] static bool append(test_reflection::custom_sequence<T>& values,
                                   const element_type& value) {
    if constexpr (std::is_copy_constructible_v<T>) {
      values.values.push_back(value);
      return true;
    } else {
      return false;
    }
  }
};

} // namespace caors

namespace {

using test_reflection::custom_inline_wrapper;
using test_reflection::custom_object_ref;

struct actor {
  int id = 0;
  float health = 0.0F;
};

struct registry_actor {
  int id = 0;
  float health = 0.0F;
};

struct view_actor {
  int id = 0;
  float health = 0.0F;
};

struct parent_actor {
  int id = 0;
};

struct child_player {
  int level = 0;
};

struct duplicate_actor {
  int id = 0;
};

struct invalid_field_owner {
  int id = 0;
};

struct duplicate_field_owner {
  int id = 0;
  float health = 0.0F;
};

struct inconsistent_actor {
  int id = 0;
};

struct builder_actor {
  int id = 0;
  float health = 0.0F;
};

struct inventory {
  int count = 0;
};

struct inventory_holder {
  inventory items{};
};

struct vector_holder {
  caors::vector<int> values{};
};

struct list_holder {
  caors::list<int> values{};
};

struct custom_sequence_holder {
  test_reflection::custom_sequence<int> values{};
};

struct hash_map_holder {
  caors::hash_map<int, int> values{};
};

struct hash_map_ops_holder {
  int prefix = 0;
  caors::hash_map<int, int> values{};
};

struct hash_set_holder {
  caors::hash_set<int> values{};
};

struct readonly_actor {
  const int id = 7;
};

enum class reflected_color {
  red,
  blue,
};

constexpr caors::type_guid registry_actor_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x49, 0xd0, 0x23, 0x6f, 0x74, 0xb5, 0x41, 0x3f, 0x99, 0x37, 0xfa, 0x9f,
                            0xf8, 0xa6, 0x36, 0x01})};

constexpr caors::type_guid view_actor_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x49, 0xd0, 0x23, 0x6f, 0x74, 0xb5, 0x41, 0x3f, 0x99, 0x37, 0xfa, 0x9f,
                            0xf8, 0xa6, 0x36, 0x02})};

[[nodiscard]] caors::reflection_type_data make_actor_reflection_data() {
  return caors::make_reflection_type_data({
      caors::make_reflection_field_data<int>("id", "int", offsetof(actor, id)),
      caors::make_reflection_field_data<float>("health", "float", offsetof(actor, health)),
  });
}

[[nodiscard]] caors::type_id registered_type_id(std::string_view name) {
  const auto type = caors::find_type(name);
  REQUIRE(type.has_value());
  return type->id();
}

[[nodiscard]] constexpr caors::error reflection_error(caors::errc code) noexcept {
  return {caors::error_domain::reflection, code};
}

[[nodiscard]] const caors::meta_type_semantics_data&
registered_traits(const caors::reflection_field_data& field) {
  const auto* traits =
      caors::get_meta_registry().find_ptr<caors::meta_type_semantics_data>(field.type);
  REQUIRE(traits != nullptr);
  return *traits;
}

} // namespace

TEST_CASE("reflection - field info describes field layout", "[reflection]") {
  const caors::reflection_field_data field =
      caors::make_reflection_field_data<float>("health", "float", offsetof(actor, health));

  CHECK(field.valid());
  CHECK(field.name == "health");
  CHECK(field.type == registered_type_id("float"));
  CHECK(field.offset == offsetof(actor, health));
  CHECK(field.writable);

  const auto type = caors::find_type(field.type);
  REQUIRE(type.has_value());
  CHECK(type->name() == "float");
  CHECK(type->size() == sizeof(float));
  CHECK(type->alignment() == alignof(float));
}

TEST_CASE("reflection - field info describes field type traits", "[reflection]") {
  const caors::reflection_field_data value =
      caors::make_reflection_field_data<int>("value", "int", 0);
  const caors::meta_type_semantics_data& value_traits = registered_traits(value);
  CHECK(value_traits.kind == caors::type_kind::value);
  CHECK(value_traits.wrapper_semantics == caors::wrapper_semantics::none);
  CHECK(value_traits.value_type == registered_type_id("int"));
  CHECK(value_traits.pointer_depth == 0);
  CHECK_FALSE(value_traits.nullable);

  const caors::reflection_field_data enumeration =
      caors::make_reflection_field_data<reflected_color>("color", "caors.test.reflection.color", 0);
  const caors::meta_type_semantics_data& enumeration_traits = registered_traits(enumeration);
  CHECK(enumeration_traits.kind == caors::type_kind::enumeration);
  CHECK(enumeration_traits.wrapper_semantics == caors::wrapper_semantics::none);
  CHECK(enumeration_traits.value_type == registered_type_id("caors.test.reflection.color"));
  CHECK_FALSE(enumeration_traits.nullable);

  const caors::reflection_field_data raw =
      caors::make_reflection_field_data<int*>("raw", "int*", 0);
  const caors::meta_type_semantics_data& raw_traits = registered_traits(raw);
  CHECK(raw_traits.kind == caors::type_kind::raw_pointer);
  CHECK(raw_traits.wrapper_semantics == caors::wrapper_semantics::inline_value);
  CHECK(raw_traits.value_type == registered_type_id("int"));
  CHECK(raw_traits.pointer_depth == 1);
  CHECK(raw_traits.nullable);

  const caors::reflection_field_data unique =
      caors::make_reflection_field_data<std::unique_ptr<int>>("unique", "std.unique_ptr<int>", 0);
  const caors::meta_type_semantics_data& unique_traits = registered_traits(unique);
  CHECK(unique_traits.kind == caors::type_kind::wrapper);
  CHECK(unique_traits.wrapper_semantics == caors::wrapper_semantics::inline_value);
  CHECK(unique_traits.value_type == registered_type_id("int"));
  CHECK(unique_traits.nullable);

  const caors::reflection_field_data shared =
      caors::make_reflection_field_data<std::shared_ptr<int>>("shared", "std.shared_ptr<int>", 0);
  const caors::meta_type_semantics_data& shared_traits = registered_traits(shared);
  CHECK(shared_traits.kind == caors::type_kind::wrapper);
  CHECK(shared_traits.wrapper_semantics == caors::wrapper_semantics::inline_value);
  CHECK(shared_traits.value_type == registered_type_id("int"));
  CHECK(shared_traits.nullable);

  const caors::reflection_field_data weak =
      caors::make_reflection_field_data<std::weak_ptr<int>>("weak", "std.weak_ptr<int>", 0);
  const caors::meta_type_semantics_data& weak_traits = registered_traits(weak);
  CHECK(weak_traits.kind == caors::type_kind::wrapper);
  CHECK(weak_traits.wrapper_semantics == caors::wrapper_semantics::inline_value);
  CHECK(weak_traits.value_type == registered_type_id("int"));
  CHECK(weak_traits.nullable);

  const caors::reflection_field_data optional =
      caors::make_reflection_field_data<std::optional<int>>("optional", "std.optional<int>", 0);
  const caors::meta_type_semantics_data& optional_traits = registered_traits(optional);
  CHECK(optional_traits.kind == caors::type_kind::wrapper);
  CHECK(optional_traits.wrapper_semantics == caors::wrapper_semantics::inline_value);
  CHECK(optional_traits.value_type == registered_type_id("int"));
  CHECK(optional_traits.nullable);

  const caors::reflection_field_data wrapper =
      caors::make_reflection_field_data<std::reference_wrapper<int>>(
          "wrapper", "std.reference_wrapper<int>", 0);
  const caors::meta_type_semantics_data& wrapper_traits = registered_traits(wrapper);
  CHECK(wrapper_traits.kind == caors::type_kind::wrapper);
  CHECK(wrapper_traits.wrapper_semantics == caors::wrapper_semantics::inline_value);
  CHECK(wrapper_traits.value_type == registered_type_id("int"));
  CHECK_FALSE(wrapper_traits.nullable);

  const caors::reflection_field_data custom_inline =
      caors::make_reflection_field_data<custom_inline_wrapper<int>>("custom_inline",
                                                                    "custom.inline<int>", 0);
  const caors::meta_type_semantics_data& custom_inline_traits = registered_traits(custom_inline);
  CHECK(custom_inline_traits.kind == caors::type_kind::wrapper);
  CHECK(custom_inline_traits.wrapper_semantics == caors::wrapper_semantics::inline_value);
  CHECK(custom_inline_traits.value_type == registered_type_id("int"));
  CHECK_FALSE(custom_inline_traits.nullable);

  const caors::reflection_field_data custom_ref =
      caors::make_reflection_field_data<custom_object_ref<int>>("custom_ref",
                                                                "custom.object_ref<int>", 0);
  const caors::meta_type_semantics_data& custom_ref_traits = registered_traits(custom_ref);
  CHECK(custom_ref_traits.kind == caors::type_kind::wrapper);
  CHECK(custom_ref_traits.wrapper_semantics == caors::wrapper_semantics::object_ref);
  CHECK(custom_ref_traits.value_type == registered_type_id("int"));
  CHECK(custom_ref_traits.nullable);

  const caors::reflection_field_data sequence =
      caors::make_reflection_field_data<caors::vector<int>>("values", "caors.vector<int>", 0);
  const caors::meta_type_semantics_data& sequence_traits = registered_traits(sequence);
  CHECK(sequence_traits.kind == caors::type_kind::sequence);
  CHECK(sequence_traits.wrapper_semantics == caors::wrapper_semantics::none);
  CHECK(sequence_traits.value_type == registered_type_id("int"));
}

TEST_CASE("reflection - field info reads and writes field values", "[reflection]") {
  actor instance{1, 25.0F};
  const caors::reflection_field_data id =
      caors::make_reflection_field_data<int>("id", "int", offsetof(actor, id));
  const caors::reflection_field_data health =
      caors::make_reflection_field_data<float>("health", "float", offsetof(actor, health));

  auto id_value = id.try_get<int>(&instance);
  REQUIRE(id_value.has_value());
  CHECK(*id_value == 1);

  CHECK(id.try_set<int>(&instance, 42));
  CHECK(instance.id == 42);

  auto health_value = health.try_get<float>(&instance);
  REQUIRE(health_value.has_value());
  CHECK(*health_value == 25.0F);

  CHECK(health.try_set<float>(&instance, 75.0F));
  CHECK(instance.health == 75.0F);

  CHECK_FALSE(id.try_get<float>(&instance).has_value());
  CHECK_FALSE(id.try_get<int>(nullptr).has_value());
  CHECK_FALSE(id.try_set<float>(&instance, 1.0F));
  CHECK_FALSE(id.try_set<int>(nullptr, 1));
}

TEST_CASE("reflection - field info rejects writes to readonly fields", "[reflection]") {
  readonly_actor instance{};
  const caors::reflection_field_data id =
      caors::make_reflection_field_data<const int>("id", "int", offsetof(readonly_actor, id));

  CHECK_FALSE(id.writable);

  auto id_value = id.try_get<int>(&instance);
  REQUIRE(id_value.has_value());
  CHECK(*id_value == 7);
  CHECK_FALSE(id.try_set<int>(&instance, 8));
}

TEST_CASE("reflection - type data stores and finds fields", "[reflection]") {
  const caors::reflection_type_data type = make_actor_reflection_data();

  CHECK(type.valid());
  CHECK(type.fields.size() == 2);

  auto health = type.find_field("health");
  REQUIRE(health.has_value());
  const auto health_type = caors::find_type(health->type);
  REQUIRE(health_type.has_value());
  CHECK(health_type->name() == "float");
  CHECK_FALSE(type.find_field("missing").has_value());
}

TEST_CASE("reflection - registry registers and finds types", "[reflection]") {
  REQUIRE(caors::register_reflection<registry_actor>(
              "caors.test.reflection.registry_actor", registry_actor_guid,
              {
                  caors::make_reflection_field_data<int>("id", "int", offsetof(registry_actor, id)),
                  caors::make_reflection_field_data<float>("health", "float",
                                                           offsetof(registry_actor, health)),
              }) == caors::error{});

  CHECK(caors::get_reflection_registry().size() >= 1);

  auto by_name = caors::find_reflection("caors.test.reflection.registry_actor");
  REQUIRE(by_name.has_value());
  CHECK(caors::get_reflection_registry().contains(by_name->id()));
  CHECK(by_name->field_count() == 2);

  auto by_guid = caors::find_reflection(registry_actor_guid);
  REQUIRE(by_guid.has_value());
  CHECK(by_guid->id() == by_name->id());
}

TEST_CASE("reflection - registry returns type and field views", "[reflection]") {
  const caors::error register_status = caors::register_reflection<view_actor>(
      "caors.test.reflection.view_actor", view_actor_guid,
      {
          caors::make_reflection_field_data<int>("id", "int", offsetof(view_actor, id)),
          caors::make_reflection_field_data<float>("health", "float", offsetof(view_actor, health)),
      });
  REQUIRE((register_status == caors::error{} ||
           register_status == reflection_error(caors::errc::duplicate_reflection)));

  auto reflected = caors::find_reflection("caors.test.reflection.view_actor");
  REQUIRE(reflected.has_value());
  CHECK(reflected->valid());
  CHECK(reflected->id() == registered_type_id("caors.test.reflection.view_actor"));
  CHECK(reflected->name() == "caors.test.reflection.view_actor");
  CHECK(reflected->guid() == view_actor_guid);
  CHECK(reflected->field_count() == 2);

  auto id = reflected->field_at(0);
  REQUIRE(id.has_value());
  CHECK(id->valid());
  CHECK(id->name() == "id");
  CHECK(id->type_name() == "int");
  CHECK(id->value_type() == registered_type_id("int"));
  CHECK_FALSE(reflected->field_at(99).has_value());

  auto health = reflected->find_field("health");
  REQUIRE(health.has_value());
  CHECK(health->name() == "health");
  CHECK(health->kind() == caors::type_kind::value);
  CHECK(health->wrapper_semantics() == caors::wrapper_semantics::none);
  CHECK(health->pointer_depth() == 0);
  CHECK_FALSE(health->nullable());
  CHECK(health->writable());
  CHECK(health->offset() == offsetof(view_actor, health));
  CHECK(health->size() == sizeof(float));
  CHECK(health->alignment() == alignof(float));

  view_actor instance{1, 25.0F};
  CHECK(health->try_set<float>(&instance, 90.0F));
  CHECK(instance.health == 90.0F);

  caors::any health_value = health->get_value(&instance);
  REQUIRE(health_value.valid());
  const float* copied_health = health_value.try_get<float>();
  REQUIRE(copied_health != nullptr);
  CHECK(*copied_health == 90.0F);

  const int next_id = 7;
  CHECK(id->try_set_value(&instance, caors::any::from(next_id)));
  CHECK(instance.id == 7);
}

TEST_CASE("reflection - sequence field view exposes element metadata", "[reflection]") {
  REQUIRE(caors::reflection::class_<vector_holder>("caors.test.reflection.vector_holder")
              .field("values", &vector_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::find_reflection("caors.test.reflection.vector_holder");
  REQUIRE(reflected.has_value());

  auto values = reflected->find_field("values");
  REQUIRE(values.has_value());
  CHECK(values->kind() == caors::type_kind::sequence);
  CHECK(values->is_sequence());
  CHECK(values->value_type() == registered_type_id("int"));
  CHECK(values->element_type() == registered_type_id("int"));
  CHECK(values->element_type_name() == "int");

  vector_holder instance{};
  instance.values.push_back(3);
  instance.values.push_back(5);

  caors::sequence_view sequence = values->get_ref(&instance).create_sequence_view();
  CHECK(sequence.valid());
  CHECK(sequence.writable());
  CHECK(sequence.value_type() == registered_type_id("int"));
  CHECK(sequence.value_type_name() == "int");
  CHECK(sequence.element_type() == registered_type_id("int"));
  CHECK(sequence.element_type_name() == "int");

  const std::optional<std::size_t> size = sequence.size();
  REQUIRE(size.has_value());
  CHECK(*size == 2);

  const vector_holder& readonly_instance = instance;
  caors::sequence_view readonly_sequence =
      values->get_ref(&readonly_instance).create_sequence_view();
  CHECK(readonly_sequence.valid());
  CHECK_FALSE(readonly_sequence.writable());

  caors::any_ref first = readonly_sequence.element_ref(0);
  REQUIRE(first.valid());
  CHECK_FALSE(first.writable());
  const int* first_value = first.try_get<int>();
  REQUIRE(first_value != nullptr);
  CHECK(*first_value == 3);

  caors::any_ref second = sequence.element_ref(1);
  REQUIRE(second.valid());
  CHECK(second.writable());
  CHECK(second.try_set<int>(8));
  CHECK(instance.values[1] == 8);

  CHECK(sequence.append_value(caors::any::from(13)));
  REQUIRE(instance.values.size() == 3);
  CHECK(instance.values[2] == 13);

  CHECK(sequence.resize(4));
  REQUIRE(instance.values.size() == 4);
  CHECK(instance.values[3] == 0);

  CHECK(sequence.clear());
  CHECK(instance.values.empty());
}

TEST_CASE("reflection - custom sequence traits expose field view", "[reflection]") {
  REQUIRE(caors::reflection::class_<custom_sequence_holder>(
              "caors.test.reflection.custom_sequence_holder")
              .field("values", &custom_sequence_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::find_reflection("caors.test.reflection.custom_sequence_holder");
  REQUIRE(reflected.has_value());

  auto values = reflected->find_field("values");
  REQUIRE(values.has_value());
  CHECK(values->kind() == caors::type_kind::sequence);
  CHECK(values->type_name() == "test.custom_sequence<int>");
  CHECK(values->element_type() == registered_type_id("int"));
  CHECK(values->element_type_name() == "int");

  custom_sequence_holder instance{};
  caors::sequence_view sequence = values->get_ref(&instance).create_sequence_view();
  CHECK(sequence.append_value(caors::any::from(21)));
  CHECK(sequence.append_value(caors::any::from(34)));

  const std::optional<std::size_t> size = sequence.size();
  REQUIRE(size.has_value());
  CHECK(*size == 2);

  caors::any_ref first = sequence.element_ref(0);
  REQUIRE(first.valid());
  const int* first_value = first.try_get<int>();
  REQUIRE(first_value != nullptr);
  CHECK(*first_value == 21);

  CHECK(sequence.resize(1));
  CHECK(instance.values.values.size() == 1);
  CHECK(sequence.clear());
  CHECK(instance.values.values.empty());
}

TEST_CASE("reflection - list field view uses sequence traits", "[reflection]") {
  REQUIRE(caors::reflection::class_<list_holder>("caors.test.reflection.list_holder")
              .field("values", &list_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::find_reflection("caors.test.reflection.list_holder");
  REQUIRE(reflected.has_value());

  auto values = reflected->find_field("values");
  REQUIRE(values.has_value());
  CHECK(values->kind() == caors::type_kind::sequence);
  CHECK(values->type_name() == "caors.list<int>");
  CHECK(values->element_type() == registered_type_id("int"));

  list_holder instance{};
  instance.values.push_back(3);
  instance.values.push_back(5);

  caors::sequence_view sequence = values->get_ref(&instance).create_sequence_view();
  CHECK(sequence.valid());
  CHECK(sequence.writable());

  caors::any_ref second = sequence.element_ref(1);
  REQUIRE(second.valid());
  CHECK(second.try_set(8));

  auto iterator = instance.values.begin();
  std::advance(iterator, 1);
  CHECK(*iterator == 8);

  CHECK(sequence.append_value(caors::any::from(13)));
  CHECK(instance.values.back() == 13);
}

TEST_CASE("reflection - associative field view exposes key and value metadata", "[reflection]") {
  REQUIRE(caors::reflection::class_<hash_map_holder>("caors.test.reflection.hash_map_holder")
              .field("values", &hash_map_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::find_reflection("caors.test.reflection.hash_map_holder");
  REQUIRE(reflected.has_value());

  auto values = reflected->find_field("values");
  REQUIRE(values.has_value());
  CHECK(values->kind() == caors::type_kind::associative);
  CHECK(values->is_associative());
  CHECK(values->type_name() == "caors.hash_map<int,int>");
  CHECK(values->key_type() == registered_type_id("int"));
  CHECK(values->key_type_name() == "int");
  CHECK(values->value_type() == registered_type_id("int"));
  CHECK(values->mapped_type() == registered_type_id("int"));
  CHECK(values->mapped_type_name() == "int");
}

TEST_CASE("reflection - associative field view supports dynamic operations", "[reflection]") {
  REQUIRE(
      caors::reflection::class_<hash_map_ops_holder>("caors.test.reflection.hash_map_ops_holder")
          .field("values", &hash_map_ops_holder::values)
          .commit()
          .succeeded());

  auto reflected = caors::find_reflection("caors.test.reflection.hash_map_ops_holder");
  REQUIRE(reflected.has_value());

  auto values = reflected->find_field("values");
  REQUIRE(values.has_value());

  hash_map_ops_holder instance{};
  caors::associative_view map = values->get_ref(&instance).create_associative_view();
  CHECK(map.valid());
  CHECK(map.writable());
  CHECK(map.key_type() == registered_type_id("int"));
  CHECK(map.key_type_name() == "int");
  CHECK(map.value_type() == registered_type_id("int"));
  CHECK(map.value_type_name() == "int");
  CHECK(map.mapped_type() == registered_type_id("int"));
  CHECK(map.mapped_type_name() == "int");

  CHECK(map.insert_value(caors::any::from(7), caors::any::from(70)));
  CHECK_FALSE(map.insert_value(caors::any::from(7), caors::any::from(700)));
  CHECK_FALSE(map.insert_value(caors::any::from(8.0F), caors::any::from(80)));

  const std::optional<std::size_t> size = map.size();
  REQUIRE(size.has_value());
  CHECK(*size == 1);

  const hash_map_ops_holder& readonly_instance = instance;
  caors::associative_view readonly_map =
      values->get_ref(&readonly_instance).create_associative_view();
  CHECK(readonly_map.valid());
  CHECK_FALSE(readonly_map.writable());
  CHECK_FALSE(readonly_map.insert_value(caors::any::from(8), caors::any::from(80)));
  CHECK_FALSE(readonly_map.clear());

  struct visit_state {
    int key = 0;
    int mapped = 0;
    int count = 0;
  };

  visit_state state{};
  CHECK(readonly_map.for_each_ref(
      [](caors::any_ref key, caors::any_ref mapped, void* user_data) {
        auto* state = static_cast<visit_state*>(user_data);
        const int* key_value = key.try_get<int>();
        const int* mapped_value = mapped.try_get<int>();
        if (key_value == nullptr || mapped_value == nullptr) {
          return false;
        }

        state->key = *key_value;
        state->mapped = *mapped_value;
        ++state->count;
        return true;
      },
      &state));

  CHECK(state.count == 1);
  CHECK(state.key == 7);
  CHECK(state.mapped == 70);

  CHECK(map.clear());
  const std::optional<std::size_t> empty_size = map.size();
  REQUIRE(empty_size.has_value());
  CHECK(*empty_size == 0);
  CHECK(instance.values.empty());
}

TEST_CASE("reflection - container views can be created from any_ref", "[reflection]") {
  caors::vector<int> scores{};
  caors::any_ref scores_ref = caors::any_ref::from(scores, "caors.vector<int>");
  REQUIRE(scores_ref.valid());

  caors::sequence_view sequence = scores_ref.create_sequence_view();
  CHECK(sequence.valid());
  CHECK(sequence.writable());
  CHECK(sequence.append_value(caors::any::from(10)));
  CHECK(sequence.append_value(caors::any::from(20)));
  CHECK(scores.size() == 2);
  caors::any_ref second_score = sequence.element_ref(1);
  REQUIRE(second_score.valid());
  CHECK(second_score.try_set(25));
  CHECK(scores[1] == 25);

  caors::hash_set<int> ids{};
  caors::any_ref ids_ref = caors::any_ref::from(ids, "caors.hash_set<int>");
  REQUIRE(ids_ref.valid());

  caors::set_view set = ids_ref.create_set_view();
  CHECK(set.valid());
  CHECK(set.writable());
  CHECK(set.insert_value(caors::any::from(3)));
  CHECK(ids.contains(3));

  caors::hash_map<int, int> values{};
  caors::any_ref ref = caors::any_ref::from(values, "caors.hash_map<int,int>");
  REQUIRE(ref.valid());

  caors::associative_view map = ref.create_associative_view();
  CHECK(map.valid());
  CHECK(map.writable());
  CHECK(map.insert_value(caors::any::from(3), caors::any::from(30)));
  CHECK(values.at(3) == 30);

  const caors::any_ref readonly_ref = caors::any_ref::from_const(values, "caors.hash_map<int,int>");
  caors::associative_view readonly_map = readonly_ref.create_associative_view();
  CHECK(readonly_map.valid());
  CHECK_FALSE(readonly_map.writable());
  CHECK_FALSE(readonly_map.insert_value(caors::any::from(4), caors::any::from(40)));

  caors::any copied = caors::any::from(values, "caors.hash_map<int,int>");
  caors::associative_view copied_map = copied.create_associative_view();
  CHECK(copied_map.valid());
  CHECK(copied_map.writable());
  CHECK(copied_map.insert_value(caors::any::from(5), caors::any::from(50)));

  const std::optional<std::size_t> copied_size = copied_map.size();
  REQUIRE(copied_size.has_value());
  CHECK(*copied_size == 2);
}

TEST_CASE("reflection - set field view exposes element metadata", "[reflection]") {
  REQUIRE(caors::reflection::class_<hash_set_holder>("caors.test.reflection.hash_set_holder")
              .field("values", &hash_set_holder::values)
              .commit()
              .succeeded());

  auto reflected = caors::find_reflection("caors.test.reflection.hash_set_holder");
  REQUIRE(reflected.has_value());

  auto values = reflected->find_field("values");
  REQUIRE(values.has_value());
  CHECK(values->kind() == caors::type_kind::set);
  CHECK(values->is_set());
  CHECK(values->type_name() == "caors.hash_set<int>");
  CHECK(values->value_type() == registered_type_id("int"));
  CHECK(values->element_type() == registered_type_id("int"));
  CHECK(values->element_type_name() == "int");

  hash_set_holder instance{};
  caors::set_view set = values->get_ref(&instance).create_set_view();
  CHECK(set.valid());
  CHECK(set.writable());
  CHECK(set.value_type() == registered_type_id("int"));
  CHECK(set.value_type_name() == "int");
  CHECK(set.element_type() == registered_type_id("int"));
  CHECK(set.element_type_name() == "int");
  CHECK(set.insert_value(caors::any::from(11)));
  CHECK_FALSE(set.insert_value(caors::any::from(11)));
  CHECK_FALSE(set.insert_value(caors::any::from(2.0F)));

  const std::optional<std::size_t> size = set.size();
  REQUIRE(size.has_value());
  CHECK(*size == 1);

  const hash_set_holder& readonly_instance = instance;
  caors::set_view readonly_set = values->get_ref(&readonly_instance).create_set_view();
  CHECK(readonly_set.valid());
  CHECK_FALSE(readonly_set.writable());
  CHECK_FALSE(readonly_set.insert_value(caors::any::from(12)));
  CHECK_FALSE(readonly_set.clear());

  int visited = 0;
  CHECK(readonly_set.for_each_ref(
      [](caors::any_ref element, void* user_data) {
        auto* visited = static_cast<int*>(user_data);
        const int* value = element.try_get<int>();
        if (value == nullptr) {
          return false;
        }

        *visited = *value;
        return true;
      },
      &visited));
  CHECK(visited == 11);

  CHECK(set.clear());
  CHECK(instance.values.empty());
}

TEST_CASE("reflection - views remain valid for append-only registry", "[reflection]") {
  const caors::error register_status = caors::register_reflection<view_actor>(
      "caors.test.reflection.view_actor", view_actor_guid,
      {
          caors::make_reflection_field_data<int>("id", "int", offsetof(view_actor, id)),
          caors::make_reflection_field_data<float>("health", "float", offsetof(view_actor, health)),
      });
  REQUIRE((register_status == caors::error{} ||
           register_status == reflection_error(caors::errc::duplicate_reflection)));

  auto reflected = caors::find_reflection("caors.test.reflection.view_actor");
  REQUIRE(reflected.has_value());

  auto field = reflected->find_field("id");
  REQUIRE(field.has_value());
  CHECK(reflected->valid());
  CHECK(field->valid());

  CHECK(caors::register_reflection<view_actor>(
            "caors.test.reflection.view_actor", view_actor_guid,
            {
                caors::make_reflection_field_data<int>("id", "int", offsetof(view_actor, id)),
                caors::make_reflection_field_data<float>("health", "float",
                                                         offsetof(view_actor, health)),
            }) == reflection_error(caors::errc::duplicate_reflection));

  CHECK(reflected->valid());
  CHECK(field->valid());
}

TEST_CASE("reflection - registry registers child type with parent", "[reflection]") {
  REQUIRE(caors::register_reflection<parent_actor>(
              "caors.test.reflection.parent_actor",
              {
                  caors::make_reflection_field_data<int>("id", "int", offsetof(parent_actor, id)),
              }) == caors::error{});
  const auto reflected_actor = caors::find_reflection("caors.test.reflection.parent_actor");
  REQUIRE(reflected_actor.has_value());

  REQUIRE(
      caors::register_reflection<child_player>(
          "caors.test.reflection.child_player",
          {
              caors::make_reflection_field_data<int>("level", "int", offsetof(child_player, level)),
          },
          reflected_actor->id()) == caors::error{});

  auto reflected_player = caors::find_reflection("caors.test.reflection.child_player");
  REQUIRE(reflected_player.has_value());
  CHECK(reflected_player->info().parent_id() == reflected_actor->id());
}

TEST_CASE("reflection - registry reports duplicate reflection", "[reflection]") {
  REQUIRE(
      caors::register_reflection<duplicate_actor>(
          "caors.test.reflection.duplicate_actor",
          {
              caors::make_reflection_field_data<int>("id", "int", offsetof(duplicate_actor, id)),
          }) == caors::error{});
  CHECK(caors::register_reflection<duplicate_actor>(
            "caors.test.reflection.duplicate_actor",
            {
                caors::make_reflection_field_data<int>("id", "int", offsetof(duplicate_actor, id)),
            }) == reflection_error(caors::errc::duplicate_reflection));
}

TEST_CASE("reflection - registry validates fields", "[reflection]") {
  CHECK(caors::register_reflection<invalid_field_owner>("caors.test.reflection.invalid_field",
                                                        {
                                                            caors::reflection_field_data{},
                                                        }) ==
        reflection_error(caors::errc::invalid_field));

  CHECK(caors::register_reflection<duplicate_field_owner>(
            "caors.test.reflection.duplicate_field",
            {
                caors::make_reflection_field_data<int>("id", "int",
                                                       offsetof(duplicate_field_owner, id)),
                caors::make_reflection_field_data<float>("id", "float",
                                                         offsetof(duplicate_field_owner, health)),
            }) == reflection_error(caors::errc::duplicate_field));
}

TEST_CASE("reflection - registry reports inconsistent existing type", "[reflection]") {
  REQUIRE(
      caors::register_reflection<inconsistent_actor>(
          "caors.test.reflection.inconsistent_actor",
          {
              caors::make_reflection_field_data<int>("id", "int", offsetof(inconsistent_actor, id)),
          }) == caors::error{});

  const caors::meta_type_data inconsistent_type{
      caors::type_id::invalid(),   caors::string_name{"caors.test.reflection.inconsistent_actor"},
      caors::type_guid::nil(),     sizeof(inconsistent_actor) + 1,
      alignof(inconsistent_actor), caors::type_id::invalid(),
  };
  const caors::reflection_type_data inconsistent_reflection = caors::make_reflection_type_data({
      caors::make_reflection_field_data<int>("id", "int", offsetof(inconsistent_actor, id)),
  });
  CHECK(caors::get_reflection_registry().register_type_for<inconsistent_actor>(
            inconsistent_type, inconsistent_reflection) ==
        reflection_error(caors::errc::inconsistent_type));
}

TEST_CASE("reflection - class builder infers built-in field types", "[reflection]") {
  const caors::error result =
      caors::reflection::class_<builder_actor>("caors.test.reflection.builder_actor")
          .field("id", &builder_actor::id)
          .field("health", &builder_actor::health)
          .commit();

  REQUIRE(result == caors::error{});

  auto reflected = caors::find_reflection("caors.test.reflection.builder_actor");
  REQUIRE(reflected.has_value());

  auto id = reflected->find_field("id");
  REQUIRE(id.has_value());
  CHECK(id->type_name() == "int");
  CHECK(id->type() == registered_type_id("int"));

  auto health = reflected->find_field("health");
  REQUIRE(health.has_value());
  CHECK(health->type_name() == "float");
  CHECK(health->type() == registered_type_id("float"));

  builder_actor instance{3, 80.0F};
  CHECK(id->try_set<int>(&instance, 10));
  CHECK(health->try_set<float>(&instance, 20.0F));
  CHECK(instance.id == 10);
  CHECK(instance.health == 20.0F);
}

TEST_CASE("reflection - class builder resolves registered C++ field types", "[reflection]") {
  REQUIRE(caors::reflection::class_<inventory>("caors.test.reflection.inventory")
              .field("count", &inventory::count)
              .commit() == caors::error{});

  REQUIRE(caors::reflection::class_<inventory_holder>("caors.test.reflection.inventory_holder")
              .field("items", &inventory_holder::items)
              .commit() == caors::error{});

  auto reflected = caors::find_reflection("caors.test.reflection.inventory_holder");
  REQUIRE(reflected.has_value());

  auto items = reflected->find_field("items");
  REQUIRE(items.has_value());
  CHECK(items->type_name() == "caors.test.reflection.inventory");
  CHECK(items->type() == registered_type_id("caors.test.reflection.inventory"));
}

TEST_CASE("reflection - class builder names anonymous C++ field types", "[reflection]") {
  const caors::type_id anonymous_id = caors::type_id_of<test_reflection::named_field_value>();
  auto anonymous = caors::find_type<test_reflection::named_field_value>();
  REQUIRE(anonymous.has_value());
  CHECK(anonymous->id() == anonymous_id);
  CHECK(anonymous->name().empty());

  REQUIRE(caors::reflection::class_<test_reflection::named_field_owner>(
              "caors.test.reflection.named_field_owner")
              .field("value", &test_reflection::named_field_owner::value)
              .commit() == caors::error{});

  auto value_type = caors::find_type("caors.test.reflection.named_field_value");
  REQUIRE(value_type.has_value());
  CHECK(value_type->id() == anonymous_id);

  auto reflected = caors::find_reflection("caors.test.reflection.named_field_owner");
  REQUIRE(reflected.has_value());

  auto value = reflected->find_field("value");
  REQUIRE(value.has_value());
  CHECK(value->type_name() == "caors.test.reflection.named_field_value");
  CHECK(value->type() == anonymous_id);
}

TEST_CASE("reflection - class builder reports missing field type mapping", "[reflection]") {
  struct unregistered_field {};
  struct owner {
    unregistered_field field{};
  };

  CHECK(caors::reflection::class_<owner>("caors.test.reflection.owner")
            .field("field", &owner::field)
            .commit() == reflection_error(caors::errc::invalid_field));
}
