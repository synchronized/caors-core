// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：meta_property_field_view_describes_composite_traits_test.cpp

#include "meta_property_test_support.h"

using namespace test_meta;

TEST_CASE("meta - property view describes composite traits", "[meta]") {
  const caors::meta::type reflected = require_property_traits_owner_meta();

  const caors::meta::property pair = require_property(reflected, "pair");
  CHECK(pair.kind() == caors::meta::type_kind::pair);
  CHECK(pair.get_key_type().id() == registered_type_id("int"));
  CHECK(pair.get_value_type().id() == registered_type_id("caors.string"));

  const caors::meta::property tuple = require_property(reflected, "tuple");
  CHECK(tuple.kind() == caors::meta::type_kind::tuple);
  CHECK_FALSE(static_cast<bool>(tuple.get_key_type().id()));
  CHECK_FALSE(static_cast<bool>(tuple.get_value_type().id()));

  const caors::meta::property variant = require_property(reflected, "variant");
  CHECK(variant.kind() == caors::meta::type_kind::variant);
  CHECK_FALSE(static_cast<bool>(variant.get_key_type().id()));
  CHECK_FALSE(static_cast<bool>(variant.get_value_type().id()));
}

// 原测试文件：meta_property_field_view_describes_field_layout_test.cpp

#include "meta_property_test_support.h"

using namespace test_meta;

TEST_CASE("meta - property view describes field layout", "[meta]") {
  REQUIRE(caors::registration::type_<field_layout_actor>("caors.test.meta.field_layout_actor")
              .meta()
              .property("health", &field_layout_actor::health)
              .commit() == caors::error{});

  const caors::meta::type reflected = require_reflected("caors.test.meta.field_layout_actor");
  const caors::meta::property field = require_property(reflected, "health");

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

// 原测试文件：meta_property_field_view_exposes_builtin_schema_test.cpp

#include "meta_property_test_support.h"

using namespace test_meta;

namespace {

struct schema_actor {
  caors::string format{};
  int priority = 0;
};

} // namespace

TEST_CASE("meta - property view exposes builtin schema", "[meta]") {
  REQUIRE(
      caors::registration::type_<schema_actor>("caors.test.meta.schema_actor")
          .meta()
          .property("format", &schema_actor::format, caors::meta::registration::required(),
                    caors::meta::registration::non_empty(),
                    caors::meta::registration::one_of({"json", "binary"}))
          .property("priority", &schema_actor::priority, caors::meta::registration::range(1, 100))
          .commit() == caors::error{});

  const caors::meta::type reflected = require_reflected("caors.test.meta.schema_actor");

  const caors::meta::property format = require_property(reflected, "format");
  const caors::meta::property_schema& format_schema = format.schema();
  CHECK(format_schema.required);
  CHECK(format_schema.non_empty);
  REQUIRE(format_schema.one_of_value() != nullptr);
  const caors::meta::metadata_value::array_type* allowed =
      format_schema.one_of_value()->try_as_array();
  REQUIRE(allowed != nullptr);
  REQUIRE(allowed->size() == 2);
  REQUIRE((*allowed)[0].try_as_string() != nullptr);
  REQUIRE((*allowed)[1].try_as_string() != nullptr);
  CHECK(*(*allowed)[0].try_as_string() == "json");
  CHECK(*(*allowed)[1].try_as_string() == "binary");

  const caors::meta::property priority = require_property(reflected, "priority");
  const caors::meta::property_schema& priority_schema = priority.schema();
  CHECK(priority_schema.has_range());
  REQUIRE(priority_schema.range_min_value() != nullptr);
  REQUIRE(priority_schema.range_max_value() != nullptr);
  REQUIRE(priority_schema.range_min_value()->try_as_i64() != nullptr);
  REQUIRE(priority_schema.range_max_value()->try_as_i64() != nullptr);
  CHECK(*priority_schema.range_min_value()->try_as_i64() == 1);
  CHECK(*priority_schema.range_max_value()->try_as_i64() == 100);
}

// 原测试文件：meta_property_field_view_describes_indirect_wrapper_traits_test.cpp

#include "meta_property_test_support.h"

using namespace test_meta;

TEST_CASE("meta - property view describes indirect wrapper traits", "[meta]") {
  const caors::meta::type reflected = require_property_traits_owner_meta();

  const caors::meta::property raw = require_property(reflected, "raw");
  CHECK(raw.kind() == caors::meta::type_kind::raw_pointer);
  CHECK(raw.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(raw.get_value_type().id() == registered_type_id("int"));
  CHECK(raw.indirect());
  CHECK(raw.nullable());

  const caors::meta::property unique = require_property(reflected, "unique");
  CHECK(unique.kind() == caors::meta::type_kind::wrapper);
  CHECK(unique.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(unique.get_value_type().id() == registered_type_id("int"));
  CHECK(unique.nullable());

  const caors::meta::property shared = require_property(reflected, "shared");
  CHECK(shared.kind() == caors::meta::type_kind::wrapper);
  CHECK(shared.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(shared.get_value_type().id() == registered_type_id("int"));
  CHECK(shared.nullable());

  const caors::meta::property weak = require_property(reflected, "weak");
  CHECK(weak.kind() == caors::meta::type_kind::wrapper);
  CHECK(weak.wrapper_semantics() == caors::meta::wrapper_semantics::unsupported);
  CHECK(weak.get_value_type().id() == registered_type_id("int"));
  CHECK(weak.nullable());

  const caors::meta::property optional = require_property(reflected, "optional");
  CHECK(optional.kind() == caors::meta::type_kind::wrapper);
  CHECK(optional.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(optional.get_value_type().id() == registered_type_id("int"));
  CHECK(optional.nullable());

  const caors::meta::property wrapper = require_property(reflected, "wrapper");
  CHECK(wrapper.kind() == caors::meta::type_kind::wrapper);
  CHECK(wrapper.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(wrapper.get_value_type().id() == registered_type_id("int"));
  CHECK_FALSE(wrapper.nullable());

  const caors::meta::property custom_inline = require_property(reflected, "custom_inline");
  CHECK(custom_inline.kind() == caors::meta::type_kind::wrapper);
  CHECK(custom_inline.wrapper_semantics() == caors::meta::wrapper_semantics::inline_value);
  CHECK(custom_inline.get_value_type().id() == registered_type_id("int"));
  CHECK_FALSE(custom_inline.nullable());
}

// 原测试文件：meta_property_field_view_describes_object_ptr_traits_test.cpp

#include "meta_property_test_support.h"

using namespace test_meta;

TEST_CASE("meta - property view describes object ptr traits", "[meta]") {
  const caors::meta::type reflected = require_property_traits_owner_meta();
  const caors::meta::property object_ptr = require_property(reflected, "object_ptr");

  CHECK(object_ptr.kind() == caors::meta::type_kind::value);
  CHECK(object_ptr.wrapper_semantics() == caors::meta::wrapper_semantics::none);
  CHECK(object_ptr.get_value_type().id() ==
        registered_type_id("caors.object_ptr<caors.test.meta.reflected_object_ptr_target>"));
  CHECK_FALSE(object_ptr.nullable());
}

// 原测试文件：meta_property_field_view_describes_sequence_traits_test.cpp

#include "meta_property_test_support.h"

using namespace test_meta;

TEST_CASE("meta - property view describes sequence traits", "[meta]") {
  const caors::meta::type reflected = require_property_traits_owner_meta();

  const caors::meta::property sequence = require_property(reflected, "sequence");
  CHECK(sequence.kind() == caors::meta::type_kind::sequence);
  CHECK(sequence.get_value_type().id() == registered_type_id("int"));
  CHECK(sequence.get_element_type().id() == registered_type_id("int"));
  CHECK(sequence.get_value_type().id() == registered_type_id("int"));

  const caors::meta::property span = require_property(reflected, "span");
  CHECK(span.kind() == caors::meta::type_kind::sequence);
  CHECK(span.get_value_type().id() == registered_type_id("int"));
}

// 原测试文件：meta_property_field_view_describes_value_and_enum_traits_test.cpp

#include "meta_property_test_support.h"

using namespace test_meta;

TEST_CASE("meta - property view describes value and enum traits", "[meta]") {
  const caors::meta::type reflected = require_property_traits_owner_meta();

  const caors::meta::property value = require_property(reflected, "value");
  CHECK(value.kind() == caors::meta::type_kind::value);
  CHECK(value.wrapper_semantics() == caors::meta::wrapper_semantics::none);
  CHECK(value.get_value_type().id() == registered_type_id("int"));
  CHECK(value.get_value_type().id() == registered_type_id("int"));
  CHECK_FALSE(value.indirect());
  CHECK_FALSE(value.nullable());

  const caors::meta::property enumeration = require_property(reflected, "color");
  CHECK(enumeration.kind() == caors::meta::type_kind::enumeration);
  CHECK(enumeration.get_value_type().id() == registered_type_id("caors.test.meta.color"));
  CHECK_FALSE(enumeration.nullable());
}

// 原测试文件：meta_property_field_view_reads_and_writes_field_values_test.cpp

#include "meta_property_test_support.h"

using namespace test_meta;

TEST_CASE("meta - property view reads and writes field values", "[meta]") {
  REQUIRE(caors::registration::type_<read_write_actor>("caors.test.meta.read_write_actor")
              .meta()
              .property("id", &read_write_actor::id)
              .property("health", &read_write_actor::health)
              .commit() == caors::error{});

  const caors::meta::type reflected = require_reflected("caors.test.meta.read_write_actor");
  const caors::meta::property id = require_property(reflected, "id");
  const caors::meta::property health = require_property(reflected, "health");

  read_write_actor instance{1, 25.0F};

  auto id_value = id.try_get<int>(caors::meta::view(instance));
  REQUIRE(id_value.has_value());
  CHECK(*id_value == 1);

  CHECK(id.try_set<int>(caors::meta::view(instance), 42));
  CHECK(instance.id == 42);

  auto health_value = health.try_get<float>(caors::meta::view(instance));
  REQUIRE(health_value.has_value());
  CHECK(*health_value == 25.0F);

  CHECK(health.try_set<float>(caors::meta::view(instance), 75.0F));
  CHECK(instance.health == 75.0F);

  CHECK_FALSE(id.try_get<float>(caors::meta::view(instance)).has_value());
  CHECK_FALSE(id.try_get<int>(caors::meta::instance{}).has_value());
  CHECK_FALSE(id.try_set<float>(caors::meta::view(instance), 1.0F));
  CHECK_FALSE(id.try_set<int>(caors::meta::instance{}, 1));
}

namespace {

struct accessor_property_actor {
  int health = 10;
  int level = 7;

  [[nodiscard]] int get_health() const noexcept { return health; }
  bool set_health(int value) noexcept {
    health = value;
    return true;
  }

  [[nodiscard]] const int& get_level() const noexcept { return level; }
};

} // namespace

TEST_CASE("meta - property view supports getter setter properties", "[meta]") {
  REQUIRE(
      caors::registration::type_<accessor_property_actor>("caors.test.meta.accessor_property_actor")
          .meta()
          .property("health", &accessor_property_actor::get_health,
                    &accessor_property_actor::set_health)
          .property_readonly("level", &accessor_property_actor::get_level)
          .commit() == caors::error{});

  const caors::meta::type reflected = require_reflected("caors.test.meta.accessor_property_actor");
  const caors::meta::property health = require_property(reflected, "health");
  const caors::meta::property level = require_property(reflected, "level");

  accessor_property_actor instance{};

  CHECK(health.writable());
  caors::meta::value health_value = health.get_value(caors::meta::view(instance));
  REQUIRE(health_value.is_i64());
  CHECK(health_value.as_i64() == 10);
  CHECK_FALSE(health.get_ref(caors::meta::view(instance)));

  CHECK(health.try_set_value(caors::meta::view(instance), caors::meta::value{42}));
  CHECK(instance.health == 42);
  CHECK_FALSE(health.try_set_any(caors::meta::view(instance), caors::meta::any::from(3.0F)));

  CHECK_FALSE(level.writable());
  const caors::meta::any_ref level_ref = level.get_ref(caors::meta::view(instance));
  REQUIRE(level_ref);
  REQUIRE(level_ref.try_get<int>() != nullptr);
  CHECK(*level_ref.try_get<int>() == 7);
  CHECK_FALSE(level.try_set_value(caors::meta::view(instance), caors::meta::value{11}));
}

// 原测试文件：meta_property_field_view_rejects_writes_to_readonly_fields_test.cpp

#include "meta_property_test_support.h"

using namespace test_meta;

TEST_CASE("meta - property view rejects writes to readonly fields", "[meta]") {
  REQUIRE(caors::registration::type_<readonly_actor>("caors.test.meta.readonly_actor")
              .meta()
              .property("id", &readonly_actor::id)
              .commit() == caors::error{});

  const caors::meta::property id =
      require_property(require_reflected("caors.test.meta.readonly_actor"), "id");

  readonly_actor instance{};
  CHECK_FALSE(id.writable());

  auto id_value = id.try_get<int>(caors::meta::view(instance));
  REQUIRE(id_value.has_value());
  CHECK(*id_value == 7);
  CHECK_FALSE(id.try_set<int>(caors::meta::view(instance), 8));
}

// 原测试文件：meta_property_type_view_stores_and_finds_fields_test.cpp

#include "meta_property_test_support.h"

using namespace test_meta;

TEST_CASE("meta - type view stores and finds fields", "[meta]") {
  REQUIRE(caors::registration::type_<stored_fields_actor>("caors.test.meta.stored_fields_actor")
              .meta()
              .property("id", &stored_fields_actor::id)
              .property("health", &stored_fields_actor::health)
              .commit() == caors::error{});

  const caors::meta::type type = require_reflected("caors.test.meta.stored_fields_actor");

  CHECK(type.valid());
  CHECK(type.property_count() == 2);

  auto health = type.find_property("health");
  REQUIRE(health);
  CHECK(health.get_type().name() == "float");
  CHECK_FALSE(type.find_property("missing"));
}
