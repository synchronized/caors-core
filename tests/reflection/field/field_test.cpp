// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：reflection_field_field_view_describes_composite_traits_test.cpp

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - field view describes composite traits", "[reflection]") {
  const caors::reflection::type reflected = require_field_traits_owner_reflection();

  const caors::reflection::field pair = require_field(reflected, "pair");
  CHECK(pair.kind() == caors::meta::type_kind::pair);
  CHECK(pair.get_key_type().id() == registered_type_id("int"));
  CHECK(pair.get_value_type().id() == registered_type_id("caors.string"));

  const caors::reflection::field tuple = require_field(reflected, "tuple");
  CHECK(tuple.kind() == caors::meta::type_kind::tuple);
  CHECK_FALSE(static_cast<bool>(tuple.get_key_type().id()));
  CHECK_FALSE(static_cast<bool>(tuple.get_value_type().id()));

  const caors::reflection::field variant = require_field(reflected, "variant");
  CHECK(variant.kind() == caors::meta::type_kind::variant);
  CHECK_FALSE(static_cast<bool>(variant.get_key_type().id()));
  CHECK_FALSE(static_cast<bool>(variant.get_value_type().id()));
}

// 原测试文件：reflection_field_field_view_describes_field_layout_test.cpp

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - field view describes field layout", "[reflection]") {
  REQUIRE(
      ((void)caors::meta::class_<field_layout_actor>("caors.test.reflection.field_layout_actor"),
       caors::reflection::class_<field_layout_actor>())
          .field("health", &field_layout_actor::health)
          .commit() == caors::error{});

  const caors::reflection::type reflected =
      require_reflected("caors.test.reflection.field_layout_actor");
  const caors::reflection::field field = require_field(reflected, "health");

  CHECK(field.valid());
  CHECK(field.name() == "health");
  CHECK(field.get_type().id() == registered_type_id("float"));
  CHECK(field.get_type().name() == "float");
  CHECK(field.get_type().id() == registered_type_id("float"));
  CHECK(field.offset() == offsetof(field_layout_actor, health));
  CHECK(field.writable());
  CHECK(field.get_type().name() == "float");
  CHECK(field.size() == sizeof(float));
  CHECK(field.alignment() == alignof(float));
}

// 原测试文件：reflection_field_field_view_describes_indirect_wrapper_traits_test.cpp

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - field view describes indirect wrapper traits", "[reflection]") {
  const caors::reflection::type reflected = require_field_traits_owner_reflection();

  const caors::reflection::field raw = require_field(reflected, "raw");
  CHECK(raw.kind() == caors::meta::type_kind::raw_pointer);
  CHECK(raw.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(raw.get_value_type().id() == registered_type_id("int"));
  CHECK(raw.indirect());
  CHECK(raw.nullable());

  const caors::reflection::field unique = require_field(reflected, "unique");
  CHECK(unique.kind() == caors::meta::type_kind::wrapper);
  CHECK(unique.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(unique.get_value_type().id() == registered_type_id("int"));
  CHECK(unique.nullable());

  const caors::reflection::field shared = require_field(reflected, "shared");
  CHECK(shared.kind() == caors::meta::type_kind::wrapper);
  CHECK(shared.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(shared.get_value_type().id() == registered_type_id("int"));
  CHECK(shared.nullable());

  const caors::reflection::field weak = require_field(reflected, "weak");
  CHECK(weak.kind() == caors::meta::type_kind::wrapper);
  CHECK(weak.wrapper_semantics() == caors::meta::wrapper_semantics::unsupported);
  CHECK(weak.get_value_type().id() == registered_type_id("int"));
  CHECK(weak.nullable());

  const caors::reflection::field optional = require_field(reflected, "optional");
  CHECK(optional.kind() == caors::meta::type_kind::wrapper);
  CHECK(optional.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(optional.get_value_type().id() == registered_type_id("int"));
  CHECK(optional.nullable());

  const caors::reflection::field wrapper = require_field(reflected, "wrapper");
  CHECK(wrapper.kind() == caors::meta::type_kind::wrapper);
  CHECK(wrapper.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(wrapper.get_value_type().id() == registered_type_id("int"));
  CHECK_FALSE(wrapper.nullable());

  const caors::reflection::field custom_inline = require_field(reflected, "custom_inline");
  CHECK(custom_inline.kind() == caors::meta::type_kind::wrapper);
  CHECK(custom_inline.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(custom_inline.get_value_type().id() == registered_type_id("int"));
  CHECK_FALSE(custom_inline.nullable());
}

// 原测试文件：reflection_field_field_view_describes_object_ptr_traits_test.cpp

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - field view describes object ptr traits", "[reflection]") {
  const caors::reflection::type reflected = require_field_traits_owner_reflection();
  const caors::reflection::field object_ptr = require_field(reflected, "object_ptr");

  CHECK(object_ptr.kind() == caors::meta::type_kind::value);
  CHECK(object_ptr.wrapper_semantics() == caors::meta::wrapper_semantics::none);
  CHECK(object_ptr.get_value_type().id() ==
        registered_type_id("caors.object_ptr<caors.test.reflection.reflected_object_ptr_target>"));
  CHECK_FALSE(object_ptr.nullable());
}

// 原测试文件：reflection_field_field_view_describes_sequence_traits_test.cpp

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - field view describes sequence traits", "[reflection]") {
  const caors::reflection::type reflected = require_field_traits_owner_reflection();

  const caors::reflection::field sequence = require_field(reflected, "sequence");
  CHECK(sequence.kind() == caors::meta::type_kind::sequence);
  CHECK(sequence.get_value_type().id() == registered_type_id("int"));
  CHECK(sequence.get_element_type().id() == registered_type_id("int"));
  CHECK(sequence.get_value_type().id() == registered_type_id("int"));

  const caors::reflection::field span = require_field(reflected, "span");
  CHECK(span.kind() == caors::meta::type_kind::sequence);
  CHECK(span.get_value_type().id() == registered_type_id("int"));
}

// 原测试文件：reflection_field_field_view_describes_value_and_enum_traits_test.cpp

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - field view describes value and enum traits", "[reflection]") {
  const caors::reflection::type reflected = require_field_traits_owner_reflection();

  const caors::reflection::field value = require_field(reflected, "value");
  CHECK(value.kind() == caors::meta::type_kind::value);
  CHECK(value.wrapper_semantics() == caors::meta::wrapper_semantics::none);
  CHECK(value.get_value_type().id() == registered_type_id("int"));
  CHECK(value.get_value_type().id() == registered_type_id("int"));
  CHECK_FALSE(value.indirect());
  CHECK_FALSE(value.nullable());

  const caors::reflection::field enumeration = require_field(reflected, "color");
  CHECK(enumeration.kind() == caors::meta::type_kind::enumeration);
  CHECK(enumeration.get_value_type().id() == registered_type_id("caors.test.reflection.color"));
  CHECK_FALSE(enumeration.nullable());
}

// 原测试文件：reflection_field_field_view_reads_and_writes_field_values_test.cpp

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - field view reads and writes field values", "[reflection]") {
  REQUIRE(((void)caors::meta::class_<read_write_actor>("caors.test.reflection.read_write_actor"),
           caors::reflection::class_<read_write_actor>())
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

// 原测试文件：reflection_field_field_view_rejects_writes_to_readonly_fields_test.cpp

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - field view rejects writes to readonly fields", "[reflection]") {
  REQUIRE(((void)caors::meta::class_<readonly_actor>("caors.test.reflection.readonly_actor"),
           caors::reflection::class_<readonly_actor>())
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

// 原测试文件：reflection_field_type_view_stores_and_finds_fields_test.cpp

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - type view stores and finds fields", "[reflection]") {
  REQUIRE(
      ((void)caors::meta::class_<stored_fields_actor>("caors.test.reflection.stored_fields_actor"),
       caors::reflection::class_<stored_fields_actor>())
          .field("id", &stored_fields_actor::id)
          .field("health", &stored_fields_actor::health)
          .commit() == caors::error{});

  const caors::reflection::type type =
      require_reflected("caors.test.reflection.stored_fields_actor");

  CHECK(type.valid());
  CHECK(type.field_count() == 2);

  auto health = type.find_field("health");
  REQUIRE(health);
  CHECK(health.get_type().name() == "float");
  CHECK_FALSE(type.find_field("missing"));
}
