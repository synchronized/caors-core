// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

using namespace test_reflection;
TEST_CASE("reflection - field info describes field layout", "[reflection]") {
  const caors::reflection::field_data field =
      caors::reflection::make_field_data<float>("health", "float", offsetof(actor, health));

  CHECK(field.valid());
  CHECK(field.name == "health");
  CHECK(field.type == registered_type_id("float"));
  CHECK(field.offset == offsetof(actor, health));
  CHECK(field.writable);

  const auto type = caors::meta::find(field.type);
  REQUIRE(type.has_value());
  CHECK(type->name() == "float");
  CHECK(type->size() == sizeof(float));
  CHECK(type->alignment() == alignof(float));
}

TEST_CASE("reflection - field info describes field type traits", "[reflection]") {
  const caors::reflection::field_data value =
      caors::reflection::make_field_data<int>("value", "int", 0);
  const caors::meta::type_semantics_data& value_traits = registered_traits(value);
  CHECK(value_traits.kind == caors::meta::type_kind::value);
  CHECK(value_traits.wrapper_semantics == caors::meta::wrapper_semantics::none);
  CHECK(value_traits.value_type == registered_type_id("int"));
  CHECK(value_traits.pointer_depth == 0);
  CHECK_FALSE(value_traits.nullable);

  const caors::reflection::field_data enumeration =
      caors::reflection::make_field_data<reflected_color>("color", "caors.test.reflection.color",
                                                          0);
  const caors::meta::type_semantics_data& enumeration_traits = registered_traits(enumeration);
  CHECK(enumeration_traits.kind == caors::meta::type_kind::enumeration);
  CHECK(enumeration_traits.wrapper_semantics == caors::meta::wrapper_semantics::none);
  CHECK(enumeration_traits.value_type == registered_type_id("caors.test.reflection.color"));
  CHECK_FALSE(enumeration_traits.nullable);

  const caors::reflection::field_data raw =
      caors::reflection::make_field_data<int*>("raw", "int*", 0);
  const caors::meta::type_semantics_data& raw_traits = registered_traits(raw);
  CHECK(raw_traits.kind == caors::meta::type_kind::raw_pointer);
  CHECK(raw_traits.wrapper_semantics == caors::meta::wrapper_semantics::inline_value);
  CHECK(raw_traits.value_type == registered_type_id("int"));
  CHECK(raw_traits.pointer_depth == 1);
  CHECK(raw_traits.nullable);

  const caors::reflection::field_data unique =
      caors::reflection::make_field_data<std::unique_ptr<int>>("unique", "std.unique_ptr<int>", 0);
  const caors::meta::type_semantics_data& unique_traits = registered_traits(unique);
  CHECK(unique_traits.kind == caors::meta::type_kind::wrapper);
  CHECK(unique_traits.wrapper_semantics == caors::meta::wrapper_semantics::inline_value);
  CHECK(unique_traits.value_type == registered_type_id("int"));
  CHECK(unique_traits.nullable);

  const caors::reflection::field_data shared =
      caors::reflection::make_field_data<std::shared_ptr<int>>("shared", "std.shared_ptr<int>", 0);
  const caors::meta::type_semantics_data& shared_traits = registered_traits(shared);
  CHECK(shared_traits.kind == caors::meta::type_kind::wrapper);
  CHECK(shared_traits.wrapper_semantics == caors::meta::wrapper_semantics::inline_value);
  CHECK(shared_traits.value_type == registered_type_id("int"));
  CHECK(shared_traits.nullable);

  const caors::reflection::field_data weak =
      caors::reflection::make_field_data<std::weak_ptr<int>>("weak", "std.weak_ptr<int>", 0);
  const caors::meta::type_semantics_data& weak_traits = registered_traits(weak);
  CHECK(weak_traits.kind == caors::meta::type_kind::wrapper);
  CHECK(weak_traits.wrapper_semantics == caors::meta::wrapper_semantics::unsupported);
  CHECK(weak_traits.value_type == registered_type_id("int"));
  CHECK(weak_traits.nullable);

  const caors::reflection::field_data optional =
      caors::reflection::make_field_data<std::optional<int>>("optional", "std.optional<int>", 0);
  const caors::meta::type_semantics_data& optional_traits = registered_traits(optional);
  CHECK(optional_traits.kind == caors::meta::type_kind::wrapper);
  CHECK(optional_traits.wrapper_semantics == caors::meta::wrapper_semantics::inline_value);
  CHECK(optional_traits.value_type == registered_type_id("int"));
  CHECK(optional_traits.nullable);

  const caors::reflection::field_data wrapper =
      caors::reflection::make_field_data<std::reference_wrapper<int>>(
          "wrapper", "std.reference_wrapper<int>", 0);
  const caors::meta::type_semantics_data& wrapper_traits = registered_traits(wrapper);
  CHECK(wrapper_traits.kind == caors::meta::type_kind::wrapper);
  CHECK(wrapper_traits.wrapper_semantics == caors::meta::wrapper_semantics::inline_value);
  CHECK(wrapper_traits.value_type == registered_type_id("int"));
  CHECK_FALSE(wrapper_traits.nullable);

  const caors::reflection::field_data custom_inline =
      caors::reflection::make_field_data<custom_inline_wrapper<int>>("custom_inline",
                                                                     "custom.inline<int>", 0);
  const caors::meta::type_semantics_data& custom_inline_traits = registered_traits(custom_inline);
  CHECK(custom_inline_traits.kind == caors::meta::type_kind::wrapper);
  CHECK(custom_inline_traits.wrapper_semantics == caors::meta::wrapper_semantics::inline_value);
  CHECK(custom_inline_traits.value_type == registered_type_id("int"));
  CHECK_FALSE(custom_inline_traits.nullable);

  const caors::reflection::field_data custom_ref =
      caors::reflection::make_field_data<custom_object_ref<int>>("custom_ref",
                                                                 "custom.object_ref<int>", 0);
  const caors::meta::type_semantics_data& custom_ref_traits = registered_traits(custom_ref);
  CHECK(custom_ref_traits.kind == caors::meta::type_kind::wrapper);
  CHECK(custom_ref_traits.wrapper_semantics == caors::meta::wrapper_semantics::object_ref);
  CHECK(custom_ref_traits.value_type == registered_type_id("int"));
  CHECK(custom_ref_traits.nullable);

  const caors::reflection::field_data sequence =
      caors::reflection::make_field_data<caors::vector<int>>("values", "caors.vector<int>", 0);
  const caors::meta::type_semantics_data& sequence_traits = registered_traits(sequence);
  CHECK(sequence_traits.kind == caors::meta::type_kind::sequence);
  CHECK(sequence_traits.wrapper_semantics == caors::meta::wrapper_semantics::none);
  CHECK(sequence_traits.value_type == registered_type_id("int"));

  const caors::reflection::field_data span =
      caors::reflection::make_field_data<std::span<int>>("span", "std.span<int>", 0);
  const caors::meta::type_semantics_data& span_traits = registered_traits(span);
  CHECK(span_traits.kind == caors::meta::type_kind::sequence);
  CHECK(span_traits.wrapper_semantics == caors::meta::wrapper_semantics::none);
  CHECK(span_traits.value_type == registered_type_id("int"));

  const caors::reflection::field_data pair =
      caors::reflection::make_field_data<std::pair<int, caors::string>>(
          "pair", "std.pair<int,caors.string>", 0);
  const caors::meta::type_semantics_data& pair_traits = registered_traits(pair);
  CHECK(pair_traits.kind == caors::meta::type_kind::pair);
  CHECK(pair_traits.wrapper_semantics == caors::meta::wrapper_semantics::none);
  CHECK(pair_traits.key_type == registered_type_id("int"));
  CHECK(pair_traits.value_type == registered_type_id("caors.string"));

  const caors::reflection::field_data tuple =
      caors::reflection::make_field_data<std::tuple<int, caors::string, float>>(
          "tuple", "std.tuple<int,caors.string,float>", 0);
  const caors::meta::type_semantics_data& tuple_traits = registered_traits(tuple);
  CHECK(tuple_traits.kind == caors::meta::type_kind::tuple);
  CHECK(tuple_traits.wrapper_semantics == caors::meta::wrapper_semantics::none);
  CHECK_FALSE(static_cast<bool>(tuple_traits.key_type));
  CHECK_FALSE(static_cast<bool>(tuple_traits.value_type));

  const caors::reflection::field_data variant =
      caors::reflection::make_field_data<std::variant<int, caors::string, float>>(
          "variant", "std.variant<int,caors.string,float>", 0);
  const caors::meta::type_semantics_data& variant_traits = registered_traits(variant);
  CHECK(variant_traits.kind == caors::meta::type_kind::variant);
  CHECK(variant_traits.wrapper_semantics == caors::meta::wrapper_semantics::none);
  CHECK_FALSE(static_cast<bool>(variant_traits.key_type));
  CHECK_FALSE(static_cast<bool>(variant_traits.value_type));
}

TEST_CASE("reflection - field info reads and writes field values", "[reflection]") {
  actor instance{1, 25.0F};
  const caors::reflection::field_data id =
      caors::reflection::make_field_data<int>("id", "int", offsetof(actor, id));
  const caors::reflection::field_data health =
      caors::reflection::make_field_data<float>("health", "float", offsetof(actor, health));

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
  const caors::reflection::field_data id =
      caors::reflection::make_field_data<const int>("id", "int", offsetof(readonly_actor, id));

  CHECK_FALSE(id.writable);

  auto id_value = id.try_get<int>(&instance);
  REQUIRE(id_value.has_value());
  CHECK(*id_value == 7);
  CHECK_FALSE(id.try_set<int>(&instance, 8));
}

TEST_CASE("reflection - type data stores and finds fields", "[reflection]") {
  const caors::reflection::type_data type = make_actor_reflection_data();

  CHECK(type.valid());
  CHECK(type.fields.size() == 2);

  auto health = type.find_field("health");
  REQUIRE(health.has_value());
  const auto health_type = caors::meta::find(health->type);
  REQUIRE(health_type.has_value());
  CHECK(health_type->name() == "float");
  CHECK_FALSE(type.find_field("missing").has_value());
}
