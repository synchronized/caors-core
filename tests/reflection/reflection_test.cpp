// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

using namespace test_reflection;

namespace {

struct field_layout_actor {
  int id = 0;
  float health = 0.0F;
};

struct field_traits_owner {
  int value = 0;
  reflected_color color = reflected_color::red;
  int* raw = nullptr;
  std::unique_ptr<int> unique{};
  std::shared_ptr<int> shared{};
  std::weak_ptr<int> weak{};
  std::optional<int> optional{};
  std::reference_wrapper<int> wrapper;
  custom_inline_wrapper<int> custom_inline{};
  custom_object_ref<int> custom_ref{};
  caors::vector<int> sequence{};
  std::span<int> span{};
  std::pair<int, caors::string> pair{};
  std::tuple<int, caors::string, float> tuple{};
  std::variant<int, caors::string, float> variant{};
};

struct read_write_actor {
  int id = 0;
  float health = 0.0F;
};

struct stored_fields_actor {
  int id = 0;
  float health = 0.0F;
};

[[nodiscard]] caors::reflection::type require_reflected(std::string_view name) {
  auto reflected = caors::reflection::find(name);
  REQUIRE(reflected);
  return reflected;
}

[[nodiscard]] caors::reflection::field require_field(const caors::reflection::type& type,
                                                     std::string_view name) {
  auto field = type.find_field(name);
  REQUIRE(field);
  return field;
}

} // namespace

TEST_CASE("reflection - field view describes field layout", "[reflection]") {
  REQUIRE(caors::reflection::class_<field_layout_actor>("caors.test.reflection.field_layout_actor")
              .field("health", &field_layout_actor::health)
              .commit() == caors::error{});

  const caors::reflection::type reflected =
      require_reflected("caors.test.reflection.field_layout_actor");
  const caors::reflection::field field = require_field(reflected, "health");

  CHECK(field.valid());
  CHECK(field.name() == "health");
  CHECK(field.type() == registered_type_id("float"));
  CHECK(field.offset() == offsetof(field_layout_actor, health));
  CHECK(field.writable());
  CHECK(field.type_name() == "float");
  CHECK(field.size() == sizeof(float));
  CHECK(field.alignment() == alignof(float));
}

TEST_CASE("reflection - field view describes field type traits", "[reflection]") {
  REQUIRE(caors::reflection::class_<field_traits_owner>("caors.test.reflection.field_traits_owner")
              .field("value", &field_traits_owner::value)
              .field("color", &field_traits_owner::color)
              .field("raw", &field_traits_owner::raw)
              .field("unique", &field_traits_owner::unique)
              .field("shared", &field_traits_owner::shared)
              .field("weak", &field_traits_owner::weak)
              .field("optional", &field_traits_owner::optional)
              .field("wrapper", &field_traits_owner::wrapper)
              .field("custom_inline", &field_traits_owner::custom_inline)
              .field("custom_ref", &field_traits_owner::custom_ref)
              .field("sequence", &field_traits_owner::sequence)
              .field("span", &field_traits_owner::span)
              .field("pair", &field_traits_owner::pair)
              .field("tuple", &field_traits_owner::tuple)
              .field("variant", &field_traits_owner::variant)
              .commit() == caors::error{});

  const caors::reflection::type reflected =
      require_reflected("caors.test.reflection.field_traits_owner");

  const caors::reflection::field value = require_field(reflected, "value");
  CHECK(value.kind() == caors::meta::type_kind::value);
  CHECK(value.wrapper_semantics() == caors::meta::wrapper_semantics::none);
  CHECK(value.value_type() == registered_type_id("int"));
  CHECK(value.pointer_depth() == 0);
  CHECK_FALSE(value.nullable());

  const caors::reflection::field enumeration = require_field(reflected, "color");
  CHECK(enumeration.kind() == caors::meta::type_kind::enumeration);
  CHECK(enumeration.value_type() == registered_type_id("caors.test.reflection.color"));
  CHECK_FALSE(enumeration.nullable());

  const caors::reflection::field raw = require_field(reflected, "raw");
  CHECK(raw.kind() == caors::meta::type_kind::raw_pointer);
  CHECK(raw.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(raw.value_type() == registered_type_id("int"));
  CHECK(raw.pointer_depth() == 1);
  CHECK(raw.nullable());

  const caors::reflection::field unique = require_field(reflected, "unique");
  CHECK(unique.kind() == caors::meta::type_kind::wrapper);
  CHECK(unique.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(unique.value_type() == registered_type_id("int"));
  CHECK(unique.nullable());

  const caors::reflection::field shared = require_field(reflected, "shared");
  CHECK(shared.kind() == caors::meta::type_kind::wrapper);
  CHECK(shared.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(shared.value_type() == registered_type_id("int"));
  CHECK(shared.nullable());

  const caors::reflection::field weak = require_field(reflected, "weak");
  CHECK(weak.kind() == caors::meta::type_kind::wrapper);
  CHECK(weak.wrapper_semantics() == caors::meta::wrapper_semantics::unsupported);
  CHECK(weak.value_type() == registered_type_id("int"));
  CHECK(weak.nullable());

  const caors::reflection::field optional = require_field(reflected, "optional");
  CHECK(optional.kind() == caors::meta::type_kind::wrapper);
  CHECK(optional.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(optional.value_type() == registered_type_id("int"));
  CHECK(optional.nullable());

  const caors::reflection::field wrapper = require_field(reflected, "wrapper");
  CHECK(wrapper.kind() == caors::meta::type_kind::wrapper);
  CHECK(wrapper.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(wrapper.value_type() == registered_type_id("int"));
  CHECK_FALSE(wrapper.nullable());

  const caors::reflection::field custom_inline = require_field(reflected, "custom_inline");
  CHECK(custom_inline.kind() == caors::meta::type_kind::wrapper);
  CHECK(custom_inline.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(custom_inline.value_type() == registered_type_id("int"));
  CHECK_FALSE(custom_inline.nullable());

  const caors::reflection::field custom_ref = require_field(reflected, "custom_ref");
  CHECK(custom_ref.kind() == caors::meta::type_kind::wrapper);
  CHECK(custom_ref.wrapper_semantics() == caors::meta::wrapper_semantics::object_ref);
  CHECK(custom_ref.value_type() == registered_type_id("int"));
  CHECK(custom_ref.nullable());

  const caors::reflection::field sequence = require_field(reflected, "sequence");
  CHECK(sequence.kind() == caors::meta::type_kind::sequence);
  CHECK(sequence.value_type() == registered_type_id("int"));

  const caors::reflection::field span = require_field(reflected, "span");
  CHECK(span.kind() == caors::meta::type_kind::sequence);
  CHECK(span.value_type() == registered_type_id("int"));

  const caors::reflection::field pair = require_field(reflected, "pair");
  CHECK(pair.kind() == caors::meta::type_kind::pair);
  CHECK(pair.key_type() == registered_type_id("int"));
  CHECK(pair.value_type() == registered_type_id("caors.string"));

  const caors::reflection::field tuple = require_field(reflected, "tuple");
  CHECK(tuple.kind() == caors::meta::type_kind::tuple);
  CHECK_FALSE(static_cast<bool>(tuple.key_type()));
  CHECK_FALSE(static_cast<bool>(tuple.value_type()));

  const caors::reflection::field variant = require_field(reflected, "variant");
  CHECK(variant.kind() == caors::meta::type_kind::variant);
  CHECK_FALSE(static_cast<bool>(variant.key_type()));
  CHECK_FALSE(static_cast<bool>(variant.value_type()));
}

TEST_CASE("reflection - field view reads and writes field values", "[reflection]") {
  REQUIRE(caors::reflection::class_<read_write_actor>("caors.test.reflection.read_write_actor")
              .field("id", &read_write_actor::id)
              .field("health", &read_write_actor::health)
              .commit() == caors::error{});

  const caors::reflection::type reflected =
      require_reflected("caors.test.reflection.read_write_actor");
  const caors::reflection::field id = require_field(reflected, "id");
  const caors::reflection::field health = require_field(reflected, "health");

  read_write_actor instance{1, 25.0F};

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

TEST_CASE("reflection - field view rejects writes to readonly fields", "[reflection]") {
  REQUIRE(caors::reflection::class_<readonly_actor>("caors.test.reflection.readonly_actor")
              .field("id", &readonly_actor::id)
              .commit() == caors::error{});

  const caors::reflection::field id =
      require_field(require_reflected("caors.test.reflection.readonly_actor"), "id");

  readonly_actor instance{};
  CHECK_FALSE(id.writable());

  auto id_value = id.try_get<int>(&instance);
  REQUIRE(id_value.has_value());
  CHECK(*id_value == 7);
  CHECK_FALSE(id.try_set<int>(&instance, 8));
}

TEST_CASE("reflection - type view stores and finds fields", "[reflection]") {
  REQUIRE(
      caors::reflection::class_<stored_fields_actor>("caors.test.reflection.stored_fields_actor")
          .field("id", &stored_fields_actor::id)
          .field("health", &stored_fields_actor::health)
          .commit() == caors::error{});

  const caors::reflection::type type =
      require_reflected("caors.test.reflection.stored_fields_actor");

  CHECK(type.valid());
  CHECK(type.field_count() == 2);

  auto health = type.find_field("health");
  REQUIRE(health);
  CHECK(health.type_name() == "float");
  CHECK_FALSE(type.find_field("missing"));
}
