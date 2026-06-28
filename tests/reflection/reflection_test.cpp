// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection.h"

#include <catch2/catch_all.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>

namespace test_reflection {

template <typename T> struct custom_inline_wrapper {
  T value{};
};

template <typename T> struct custom_object_ref {
  T* value = nullptr;
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

struct readonly_actor {
  const int id = 7;
};

enum class reflected_color {
  red,
  blue,
};

constexpr caors::type_guid actor_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x54, 0x20, 0x08, 0xa6, 0x08, 0x5e, 0x48, 0xb7, 0x9c, 0x2f, 0x88, 0x68,
                            0xb2, 0x42, 0x18, 0xf3})};

constexpr caors::type_guid registry_actor_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x49, 0xd0, 0x23, 0x6f, 0x74, 0xb5, 0x41, 0x3f, 0x99, 0x37, 0xfa, 0x9f,
                            0xf8, 0xa6, 0x36, 0x01})};

constexpr caors::type_guid view_actor_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x49, 0xd0, 0x23, 0x6f, 0x74, 0xb5, 0x41, 0x3f, 0x99, 0x37, 0xfa, 0x9f,
                            0xf8, 0xa6, 0x36, 0x02})};

[[nodiscard]] caors::reflection_type make_actor_reflection() {
  return caors::make_reflection_type<actor>(
      "caors.test.reflection.actor", actor_guid,
      {
          caors::make_field_info<int>("id", "int", offsetof(actor, id)),
          caors::make_field_info<float>("health", "float", offsetof(actor, health)),
      });
}

} // namespace

TEST_CASE("reflection - field info describes field layout", "[reflection]") {
  const caors::field_info field =
      caors::make_field_info<float>("health", "float", offsetof(actor, health));

  CHECK(field.valid());
  CHECK(field.name == "health");
  CHECK(field.type == caors::make_type_id("float"));
  CHECK(field.type_name == "float");
  CHECK(field.offset == offsetof(actor, health));
  CHECK(field.size == sizeof(float));
  CHECK(field.alignment == alignof(float));
  CHECK(field.writable);
}

TEST_CASE("reflection - field info describes field type traits", "[reflection]") {
  const caors::field_info value = caors::make_field_info<int>("value", "int", 0);
  CHECK(value.kind == caors::field_kind::value);
  CHECK(value.wrapper_semantics == caors::field_wrapper_semantics::none);
  CHECK(value.value_type == caors::make_type_id("int"));
  CHECK(value.pointer_depth == 0);
  CHECK_FALSE(value.nullable);

  const caors::field_info enumeration =
      caors::make_field_info<reflected_color>("color", "caors.test.reflection.color", 0);
  CHECK(enumeration.kind == caors::field_kind::enumeration);
  CHECK(enumeration.wrapper_semantics == caors::field_wrapper_semantics::none);
  CHECK(enumeration.value_type == caors::make_type_id("caors.test.reflection.color"));
  CHECK_FALSE(enumeration.nullable);

  const caors::field_info raw = caors::make_field_info<int*>("raw", "int*", 0);
  CHECK(raw.kind == caors::field_kind::raw_pointer);
  CHECK(raw.wrapper_semantics == caors::field_wrapper_semantics::inline_value);
  CHECK(raw.value_type == caors::make_type_id("int"));
  CHECK(raw.pointer_depth == 1);
  CHECK(raw.nullable);

  const caors::field_info unique =
      caors::make_field_info<std::unique_ptr<int>>("unique", "std.unique_ptr<int>", 0);
  CHECK(unique.kind == caors::field_kind::wrapper);
  CHECK(unique.wrapper_semantics == caors::field_wrapper_semantics::inline_value);
  CHECK(unique.value_type == caors::make_type_id("int"));
  CHECK(unique.nullable);

  const caors::field_info shared =
      caors::make_field_info<std::shared_ptr<int>>("shared", "std.shared_ptr<int>", 0);
  CHECK(shared.kind == caors::field_kind::wrapper);
  CHECK(shared.wrapper_semantics == caors::field_wrapper_semantics::inline_value);
  CHECK(shared.value_type == caors::make_type_id("int"));
  CHECK(shared.nullable);

  const caors::field_info weak =
      caors::make_field_info<std::weak_ptr<int>>("weak", "std.weak_ptr<int>", 0);
  CHECK(weak.kind == caors::field_kind::wrapper);
  CHECK(weak.wrapper_semantics == caors::field_wrapper_semantics::inline_value);
  CHECK(weak.value_type == caors::make_type_id("int"));
  CHECK(weak.nullable);

  const caors::field_info optional =
      caors::make_field_info<std::optional<int>>("optional", "std.optional<int>", 0);
  CHECK(optional.kind == caors::field_kind::wrapper);
  CHECK(optional.wrapper_semantics == caors::field_wrapper_semantics::inline_value);
  CHECK(optional.value_type == caors::make_type_id("int"));
  CHECK(optional.nullable);

  const caors::field_info wrapper = caors::make_field_info<std::reference_wrapper<int>>(
      "wrapper", "std.reference_wrapper<int>", 0);
  CHECK(wrapper.kind == caors::field_kind::wrapper);
  CHECK(wrapper.wrapper_semantics == caors::field_wrapper_semantics::inline_value);
  CHECK(wrapper.value_type == caors::make_type_id("int"));
  CHECK_FALSE(wrapper.nullable);

  const caors::field_info custom_inline =
      caors::make_field_info<custom_inline_wrapper<int>>("custom_inline", "custom.inline<int>", 0);
  CHECK(custom_inline.kind == caors::field_kind::wrapper);
  CHECK(custom_inline.wrapper_semantics == caors::field_wrapper_semantics::inline_value);
  CHECK(custom_inline.value_type == caors::make_type_id("int"));
  CHECK_FALSE(custom_inline.nullable);

  const caors::field_info custom_ref =
      caors::make_field_info<custom_object_ref<int>>("custom_ref", "custom.object_ref<int>", 0);
  CHECK(custom_ref.kind == caors::field_kind::wrapper);
  CHECK(custom_ref.wrapper_semantics == caors::field_wrapper_semantics::object_ref);
  CHECK(custom_ref.value_type == caors::make_type_id("int"));
  CHECK(custom_ref.nullable);
}

TEST_CASE("reflection - field info reads and writes field values", "[reflection]") {
  actor instance{1, 25.0F};
  const caors::field_info id = caors::make_field_info<int>("id", "int", offsetof(actor, id));
  const caors::field_info health =
      caors::make_field_info<float>("health", "float", offsetof(actor, health));

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
  const caors::field_info id =
      caors::make_field_info<const int>("id", "int", offsetof(readonly_actor, id));

  CHECK_FALSE(id.writable);

  auto id_value = id.try_get<int>(&instance);
  REQUIRE(id_value.has_value());
  CHECK(*id_value == 7);
  CHECK_FALSE(id.try_set<int>(&instance, 8));
}

TEST_CASE("reflection - type stores and finds fields", "[reflection]") {
  const caors::reflection_type type = make_actor_reflection();

  CHECK(type.valid());
  CHECK(type.info.name == "caors.test.reflection.actor");
  CHECK(type.info.guid == actor_guid);
  CHECK(type.fields.size() == 2);

  auto health = type.find_field("health");
  REQUIRE(health.has_value());
  CHECK(health->type_name == "float");
  CHECK_FALSE(type.find_field("missing").has_value());
}

TEST_CASE("reflection - registry registers and finds types", "[reflection]") {
  REQUIRE(
      caors::register_reflection<registry_actor>(
          "caors.test.reflection.registry_actor", registry_actor_guid,
          {
              caors::make_field_info<int>("id", "int", offsetof(registry_actor, id)),
              caors::make_field_info<float>("health", "float", offsetof(registry_actor, health)),
          }) == caors::reflection_register_result::success);

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
  const caors::reflection_register_result register_result = caors::register_reflection<view_actor>(
      "caors.test.reflection.view_actor", view_actor_guid,
      {
          caors::make_field_info<int>("id", "int", offsetof(view_actor, id)),
          caors::make_field_info<float>("health", "float", offsetof(view_actor, health)),
      });
  REQUIRE((register_result == caors::reflection_register_result::success ||
           register_result == caors::reflection_register_result::duplicate_reflection));

  auto reflected = caors::find_reflection("caors.test.reflection.view_actor");
  REQUIRE(reflected.has_value());
  CHECK(reflected->valid());
  CHECK(reflected->id() == caors::make_type_id("caors.test.reflection.view_actor"));
  CHECK(reflected->name() == "caors.test.reflection.view_actor");
  CHECK(reflected->guid() == view_actor_guid);
  CHECK(reflected->field_count() == 2);

  auto id = reflected->field_at(0);
  REQUIRE(id.has_value());
  CHECK(id->valid());
  CHECK(id->name() == "id");
  CHECK(id->type_name() == "int");
  CHECK(id->value_type() == caors::make_type_id("int"));
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

  caors::value health_value = health->get_value(&instance);
  REQUIRE(health_value.valid());
  const float* copied_health = health_value.try_get<float>();
  REQUIRE(copied_health != nullptr);
  CHECK(*copied_health == 90.0F);

  const int next_id = 7;
  CHECK(id->try_set_value(&instance, caors::value::from(next_id)));
  CHECK(instance.id == 7);
}

TEST_CASE("reflection - views remain valid for append-only registry", "[reflection]") {
  const caors::reflection_register_result register_result = caors::register_reflection<view_actor>(
      "caors.test.reflection.view_actor", view_actor_guid,
      {
          caors::make_field_info<int>("id", "int", offsetof(view_actor, id)),
          caors::make_field_info<float>("health", "float", offsetof(view_actor, health)),
      });
  REQUIRE((register_result == caors::reflection_register_result::success ||
           register_result == caors::reflection_register_result::duplicate_reflection));

  auto reflected = caors::find_reflection("caors.test.reflection.view_actor");
  REQUIRE(reflected.has_value());

  auto field = reflected->find_field("id");
  REQUIRE(field.has_value());
  CHECK(reflected->valid());
  CHECK(field->valid());

  CHECK(caors::register_reflection<view_actor>(
            "caors.test.reflection.view_actor", view_actor_guid,
            {
                caors::make_field_info<int>("id", "int", offsetof(view_actor, id)),
                caors::make_field_info<float>("health", "float", offsetof(view_actor, health)),
            }) == caors::reflection_register_result::duplicate_reflection);

  CHECK(reflected->valid());
  CHECK(field->valid());
}

TEST_CASE("reflection - registry registers child type with parent", "[reflection]") {
  REQUIRE(caors::register_reflection<parent_actor>(
              "caors.test.reflection.parent_actor",
              {
                  caors::make_field_info<int>("id", "int", offsetof(parent_actor, id)),
              }) == caors::reflection_register_result::success);
  const auto reflected_actor = caors::find_reflection("caors.test.reflection.parent_actor");
  REQUIRE(reflected_actor.has_value());

  REQUIRE(caors::register_reflection<child_player>(
              "caors.test.reflection.child_player",
              {
                  caors::make_field_info<int>("level", "int", offsetof(child_player, level)),
              },
              reflected_actor->id()) == caors::reflection_register_result::success);

  auto reflected_player = caors::find_reflection("caors.test.reflection.child_player");
  REQUIRE(reflected_player.has_value());
  CHECK(reflected_player->info().parent_id == reflected_actor->id());
}

TEST_CASE("reflection - registry reports duplicate reflection", "[reflection]") {
  REQUIRE(caors::register_reflection<duplicate_actor>(
              "caors.test.reflection.duplicate_actor",
              {
                  caors::make_field_info<int>("id", "int", offsetof(duplicate_actor, id)),
              }) == caors::reflection_register_result::success);
  CHECK(caors::register_reflection<duplicate_actor>(
            "caors.test.reflection.duplicate_actor",
            {
                caors::make_field_info<int>("id", "int", offsetof(duplicate_actor, id)),
            }) == caors::reflection_register_result::duplicate_reflection);
}

TEST_CASE("reflection - registry validates fields", "[reflection]") {
  CHECK(caors::register_reflection<invalid_field_owner>("caors.test.reflection.invalid_field",
                                                        {
                                                            caors::field_info{},
                                                        }) ==
        caors::reflection_register_result::invalid_field);

  CHECK(
      caors::register_reflection<duplicate_field_owner>(
          "caors.test.reflection.duplicate_field",
          {
              caors::make_field_info<int>("id", "int", offsetof(duplicate_field_owner, id)),
              caors::make_field_info<float>("id", "float", offsetof(duplicate_field_owner, health)),
          }) == caors::reflection_register_result::duplicate_field);
}

TEST_CASE("reflection - registry reports inconsistent existing type", "[reflection]") {
  REQUIRE(caors::register_reflection<inconsistent_actor>(
              "caors.test.reflection.inconsistent_actor",
              {
                  caors::make_field_info<int>("id", "int", offsetof(inconsistent_actor, id)),
              }) == caors::reflection_register_result::success);

  caors::reflection_type inconsistent = caors::make_reflection_type<inconsistent_actor>(
      "caors.test.reflection.inconsistent_actor",
      {
          caors::make_field_info<int>("id", "int", offsetof(inconsistent_actor, id)),
      });
  inconsistent.info.size += 1;
  CHECK(caors::get_reflection_registry().register_type_for<inconsistent_actor>(inconsistent) ==
        caors::reflection_register_result::inconsistent_type);
}

TEST_CASE("reflection - class builder infers built-in field types", "[reflection]") {
  const caors::reflection_register_result result =
      caors::reflection::class_<builder_actor>("caors.test.reflection.builder_actor")
          .field("id", &builder_actor::id)
          .field("health", &builder_actor::health)
          .commit();

  REQUIRE(result == caors::reflection_register_result::success);

  auto reflected = caors::find_reflection("caors.test.reflection.builder_actor");
  REQUIRE(reflected.has_value());

  auto id = reflected->find_field("id");
  REQUIRE(id.has_value());
  CHECK(id->type_name() == "int");
  CHECK(id->type() == caors::make_type_id("int"));

  auto health = reflected->find_field("health");
  REQUIRE(health.has_value());
  CHECK(health->type_name() == "float");
  CHECK(health->type() == caors::make_type_id("float"));

  builder_actor instance{3, 80.0F};
  CHECK(id->try_set<int>(&instance, 10));
  CHECK(health->try_set<float>(&instance, 20.0F));
  CHECK(instance.id == 10);
  CHECK(instance.health == 20.0F);
}

TEST_CASE("reflection - class builder resolves registered C++ field types", "[reflection]") {
  REQUIRE(caors::reflection::class_<inventory>("caors.test.reflection.inventory")
              .field("count", &inventory::count)
              .commit() == caors::reflection_register_result::success);

  REQUIRE(caors::reflection::class_<inventory_holder>("caors.test.reflection.inventory_holder")
              .field("items", &inventory_holder::items)
              .commit() == caors::reflection_register_result::success);

  auto reflected = caors::find_reflection("caors.test.reflection.inventory_holder");
  REQUIRE(reflected.has_value());

  auto items = reflected->find_field("items");
  REQUIRE(items.has_value());
  CHECK(items->type_name() == "caors.test.reflection.inventory");
  CHECK(items->type() == caors::make_type_id("caors.test.reflection.inventory"));
}

TEST_CASE("reflection - class builder reports missing field type mapping", "[reflection]") {
  struct unregistered_field {};
  struct owner {
    unregistered_field field{};
  };

  CHECK(caors::reflection::class_<owner>("caors.test.reflection.owner")
            .field("field", &owner::field)
            .commit() == caors::reflection_register_result::invalid_field);
}
