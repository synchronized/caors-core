// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection.h"

#include <catch2/catch_all.hpp>

#include <cstddef>
#include <functional>
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
  static constexpr field_wrapper_semantics semantics = field_wrapper_semantics::inline_value;
  using wrapped_type = T;
};

template <typename T> struct wrapper_traits<test_reflection::custom_object_ref<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = true;
  static constexpr field_wrapper_semantics semantics = field_wrapper_semantics::object_ref;
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

struct custom_sequence_holder {
  test_reflection::custom_sequence<int> values{};
};

struct hash_map_holder {
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

} // namespace

TEST_CASE("reflection - field info describes field layout", "[reflection]") {
  const caors::reflection_field_data field =
      caors::make_reflection_field_data<float>("health", "float", offsetof(actor, health));

  CHECK(field.valid());
  CHECK(field.name == "health");
  CHECK(field.type == registered_type_id("float"));
  CHECK(field.type_name == "float");
  CHECK(field.offset == offsetof(actor, health));
  CHECK(field.size == sizeof(float));
  CHECK(field.alignment == alignof(float));
  CHECK(field.writable);
}

TEST_CASE("reflection - field info describes field type traits", "[reflection]") {
  const caors::reflection_field_data value =
      caors::make_reflection_field_data<int>("value", "int", 0);
  CHECK(value.kind == caors::field_kind::value);
  CHECK(value.wrapper_semantics == caors::field_wrapper_semantics::none);
  CHECK(value.value_type == registered_type_id("int"));
  CHECK(value.pointer_depth == 0);
  CHECK_FALSE(value.nullable);

  const caors::reflection_field_data enumeration =
      caors::make_reflection_field_data<reflected_color>("color", "caors.test.reflection.color", 0);
  CHECK(enumeration.kind == caors::field_kind::enumeration);
  CHECK(enumeration.wrapper_semantics == caors::field_wrapper_semantics::none);
  CHECK(enumeration.value_type == registered_type_id("caors.test.reflection.color"));
  CHECK_FALSE(enumeration.nullable);

  const caors::reflection_field_data raw =
      caors::make_reflection_field_data<int*>("raw", "int*", 0);
  CHECK(raw.kind == caors::field_kind::raw_pointer);
  CHECK(raw.wrapper_semantics == caors::field_wrapper_semantics::inline_value);
  CHECK(raw.value_type == registered_type_id("int"));
  CHECK(raw.pointer_depth == 1);
  CHECK(raw.nullable);

  const caors::reflection_field_data unique =
      caors::make_reflection_field_data<std::unique_ptr<int>>("unique", "std.unique_ptr<int>", 0);
  CHECK(unique.kind == caors::field_kind::wrapper);
  CHECK(unique.wrapper_semantics == caors::field_wrapper_semantics::inline_value);
  CHECK(unique.value_type == registered_type_id("int"));
  CHECK(unique.nullable);

  const caors::reflection_field_data shared =
      caors::make_reflection_field_data<std::shared_ptr<int>>("shared", "std.shared_ptr<int>", 0);
  CHECK(shared.kind == caors::field_kind::wrapper);
  CHECK(shared.wrapper_semantics == caors::field_wrapper_semantics::inline_value);
  CHECK(shared.value_type == registered_type_id("int"));
  CHECK(shared.nullable);

  const caors::reflection_field_data weak =
      caors::make_reflection_field_data<std::weak_ptr<int>>("weak", "std.weak_ptr<int>", 0);
  CHECK(weak.kind == caors::field_kind::wrapper);
  CHECK(weak.wrapper_semantics == caors::field_wrapper_semantics::inline_value);
  CHECK(weak.value_type == registered_type_id("int"));
  CHECK(weak.nullable);

  const caors::reflection_field_data optional =
      caors::make_reflection_field_data<std::optional<int>>("optional", "std.optional<int>", 0);
  CHECK(optional.kind == caors::field_kind::wrapper);
  CHECK(optional.wrapper_semantics == caors::field_wrapper_semantics::inline_value);
  CHECK(optional.value_type == registered_type_id("int"));
  CHECK(optional.nullable);

  const caors::reflection_field_data wrapper =
      caors::make_reflection_field_data<std::reference_wrapper<int>>(
          "wrapper", "std.reference_wrapper<int>", 0);
  CHECK(wrapper.kind == caors::field_kind::wrapper);
  CHECK(wrapper.wrapper_semantics == caors::field_wrapper_semantics::inline_value);
  CHECK(wrapper.value_type == registered_type_id("int"));
  CHECK_FALSE(wrapper.nullable);

  const caors::reflection_field_data custom_inline =
      caors::make_reflection_field_data<custom_inline_wrapper<int>>("custom_inline",
                                                                    "custom.inline<int>", 0);
  CHECK(custom_inline.kind == caors::field_kind::wrapper);
  CHECK(custom_inline.wrapper_semantics == caors::field_wrapper_semantics::inline_value);
  CHECK(custom_inline.value_type == registered_type_id("int"));
  CHECK_FALSE(custom_inline.nullable);

  const caors::reflection_field_data custom_ref =
      caors::make_reflection_field_data<custom_object_ref<int>>("custom_ref",
                                                                "custom.object_ref<int>", 0);
  CHECK(custom_ref.kind == caors::field_kind::wrapper);
  CHECK(custom_ref.wrapper_semantics == caors::field_wrapper_semantics::object_ref);
  CHECK(custom_ref.value_type == registered_type_id("int"));
  CHECK(custom_ref.nullable);

  const caors::reflection_field_data sequence =
      caors::make_reflection_field_data<caors::vector<int>>("values", "caors.vector<int>", 0);
  CHECK(sequence.kind == caors::field_kind::sequence);
  CHECK(sequence.wrapper_semantics == caors::field_wrapper_semantics::none);
  CHECK(sequence.element_type == registered_type_id("int"));
  CHECK(sequence.element_size == sizeof(int));
  CHECK(sequence.element_alignment == alignof(int));
  CHECK(sequence.sequence_size != nullptr);
  CHECK(sequence.sequence_const_element != nullptr);
  CHECK(sequence.sequence_mutable_element != nullptr);
  CHECK(sequence.sequence_clear != nullptr);
  CHECK(sequence.sequence_resize != nullptr);
  CHECK(sequence.sequence_append != nullptr);
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
  CHECK(health->type_name == "float");
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
  CHECK(health->kind() == caors::field_kind::value);
  CHECK(health->wrapper_semantics() == caors::field_wrapper_semantics::none);
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
  CHECK(values->kind() == caors::field_kind::sequence);
  CHECK(values->is_sequence());
  CHECK(values->element_type() == registered_type_id("int"));
  CHECK(values->element_type_name() == "int");

  vector_holder instance{};
  instance.values.push_back(3);
  instance.values.push_back(5);

  const std::optional<std::size_t> size = values->sequence_size(&instance);
  REQUIRE(size.has_value());
  CHECK(*size == 2);

  caors::any_ref first = values->sequence_element_ref(&instance, 0);
  REQUIRE(first.valid());
  const int* first_value = first.try_get<int>();
  REQUIRE(first_value != nullptr);
  CHECK(*first_value == 3);

  caors::any_ref second = values->sequence_element_mutable_ref(&instance, 1);
  REQUIRE(second.valid());
  CHECK(second.try_set<int>(8));
  CHECK(instance.values[1] == 8);

  CHECK(values->sequence_append_value(&instance, caors::any::from(13)));
  REQUIRE(instance.values.size() == 3);
  CHECK(instance.values[2] == 13);

  CHECK(values->sequence_resize(&instance, 4));
  REQUIRE(instance.values.size() == 4);
  CHECK(instance.values[3] == 0);

  CHECK(values->sequence_clear(&instance));
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
  CHECK(values->kind() == caors::field_kind::sequence);
  CHECK(values->type_name() == "test.custom_sequence<int>");
  CHECK(values->element_type() == registered_type_id("int"));
  CHECK(values->element_type_name() == "int");

  custom_sequence_holder instance{};
  CHECK(values->sequence_append_value(&instance, caors::any::from(21)));
  CHECK(values->sequence_append_value(&instance, caors::any::from(34)));

  const std::optional<std::size_t> size = values->sequence_size(&instance);
  REQUIRE(size.has_value());
  CHECK(*size == 2);

  caors::any_ref first = values->sequence_element_ref(&instance, 0);
  REQUIRE(first.valid());
  const int* first_value = first.try_get<int>();
  REQUIRE(first_value != nullptr);
  CHECK(*first_value == 21);

  CHECK(values->sequence_resize(&instance, 1));
  CHECK(instance.values.values.size() == 1);
  CHECK(values->sequence_clear(&instance));
  CHECK(instance.values.values.empty());
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
  CHECK(values->kind() == caors::field_kind::associative);
  CHECK(values->is_associative());
  CHECK(values->type_name() == "caors.hash_map<int,int>");
  CHECK(values->key_type() == registered_type_id("int"));
  CHECK(values->key_type_name() == "int");
  CHECK(values->mapped_type() == registered_type_id("int"));
  CHECK(values->mapped_type_name() == "int");
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
  CHECK(values->kind() == caors::field_kind::set);
  CHECK(values->is_set());
  CHECK(values->type_name() == "caors.hash_set<int>");
  CHECK(values->element_type() == registered_type_id("int"));
  CHECK(values->element_type_name() == "int");
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
