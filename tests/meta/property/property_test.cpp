// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：meta_property_property_view_describes_composite_traits_test.cpp

#include "caors/meta/property_accessor.h"
#include "caors/serialization/schema_query.h"
#include "meta_property_test_support.h"

using namespace test_meta;

namespace {

struct compiled_property_actor {
  int value = 7;
  const int readonly_value = 11;
};

} // namespace

TEST_CASE("meta - compiled property accessor hides node storage", "[meta]") {
  REQUIRE(
      caors::registration::type_<compiled_property_actor>("caors.test.meta.compiled_property_actor")
          .meta()
          .property("value", &compiled_property_actor::value)
          .property("readonly_value", &compiled_property_actor::readonly_value)
          .commit() == caors::error{});

  const caors::meta::type reflected = require_reflected("caors.test.meta.compiled_property_actor");
  const caors::meta::property_accessor value =
      caors::meta::compile_property_accessor(require_property(reflected, "value"));
  const caors::meta::property_accessor readonly_value =
      caors::meta::compile_property_accessor(require_property(reflected, "readonly_value"));

  REQUIRE(value);
  REQUIRE(readonly_value);
  CHECK_FALSE(caors::meta::compile_property_accessor(caors::meta::property{}));

  compiled_property_actor object{};
  const caors::meta::any_ref mutable_ref = value.get_ref(&object);
  REQUIRE(mutable_ref);
  REQUIRE(mutable_ref.try_get<int>() != nullptr);
  CHECK(mutable_ref.try_set(42));
  CHECK(object.value == 42);

  const compiled_property_actor& const_object = object;
  const caors::meta::any_ref const_ref = value.get_ref(&const_object);
  REQUIRE(const_ref);
  REQUIRE(const_ref.try_get<int>() != nullptr);
  CHECK(*const_ref.try_get<int>() == 42);
  CHECK_FALSE(const_ref.try_set(9));

  const caors::meta::any_ref readonly_ref = readonly_value.get_ref(&object);
  REQUIRE(readonly_ref);
  REQUIRE(readonly_ref.try_get<int>() != nullptr);
  CHECK(*readonly_ref.try_get<int>() == 11);
  CHECK_FALSE(readonly_ref.try_set(12));
}

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

// 原测试文件：meta_property_property_view_describes_property_layout_test.cpp

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

// 原测试文件：meta_property_property_view_exposes_builtin_schema_test.cpp

#include "meta_property_test_support.h"

using namespace test_meta;

namespace {

struct schema_actor {
  caors::string format{};
  int priority = 0;
};

} // namespace

TEST_CASE("meta - property view exposes builtin schema", "[meta]") {
  REQUIRE(caors::registration::type_<schema_actor>("caors.test.meta.schema_actor")
              .meta()
              .property("format", &schema_actor::format, caors::meta::registration::required(),
                        caors::meta::registration::non_empty(),
                        caors::meta::registration::one_of({"json", "binary"}),
                        caors::meta::registration::default_value("json"))
              .property("priority", &schema_actor::priority,
                        caors::meta::registration::range(1, 100),
                        caors::meta::registration::default_value(10))
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
  REQUIRE(format_schema.default_value_ptr() != nullptr);
  REQUIRE(format_schema.default_value_ptr()->try_as_string() != nullptr);
  CHECK(*format_schema.default_value_ptr()->try_as_string() == "json");

  const caors::meta::property priority = require_property(reflected, "priority");
  const caors::meta::property_schema& priority_schema = priority.schema();
  CHECK(priority_schema.has_range());
  REQUIRE(priority_schema.range_min_value() != nullptr);
  REQUIRE(priority_schema.range_max_value() != nullptr);
  REQUIRE(priority_schema.range_min_value()->try_as_i64() != nullptr);
  REQUIRE(priority_schema.range_max_value()->try_as_i64() != nullptr);
  CHECK(*priority_schema.range_min_value()->try_as_i64() == 1);
  CHECK(*priority_schema.range_max_value()->try_as_i64() == 100);
  REQUIRE(priority_schema.default_value_ptr() != nullptr);
  REQUIRE(priority_schema.default_value_ptr()->try_as_i64() != nullptr);
  CHECK(*priority_schema.default_value_ptr()->try_as_i64() == 10);
}

// 原测试文件：meta_property_property_view_describes_indirect_wrapper_traits_test.cpp

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

// 原测试文件：meta_property_property_view_describes_object_ptr_traits_test.cpp

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

// 原测试文件：meta_property_property_view_describes_sequence_traits_test.cpp

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

// 原测试文件：meta_property_property_view_describes_value_and_enum_traits_test.cpp

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

// 原测试文件：meta_property_property_view_reads_and_writes_property_values_test.cpp

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

struct failing_setter_property_actor {
  int value = 3;

  [[nodiscard]] int get_value() const noexcept { return value; }
  bool set_value(int) noexcept { return false; }
};

struct property_conversion_actor {
  int value = 0;
  bool enabled = false;
  double scale = 0.0;
};

enum class property_conversion_state {
  idle = 1,
  running = 2,
};

struct property_conversion_extended_actor {
  caors::core::string_name label{};
  caors::guid guid{};
  caors::type_guid type{};
  property_conversion_state state = property_conversion_state::idle;
};

struct serialized_alias_property_actor {
  int value = 0;
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

  caors::meta::instance object = caors::meta::view(instance);
  CHECK(object.set("health", 64));
  CHECK(instance.health == 64);
  CHECK(object.set("health", caors::meta::value{75}));
  CHECK(instance.health == 75);

  CHECK_FALSE(level.writable());
  const caors::meta::any_ref level_ref = level.get_ref(caors::meta::view(instance));
  REQUIRE(level_ref);
  REQUIRE(level_ref.try_get<int>() != nullptr);
  CHECK(*level_ref.try_get<int>() == 7);
  CHECK_FALSE(level.try_set_value(caors::meta::view(instance), caors::meta::value{11}));
  CHECK_FALSE(object.set("level", 11));
}

TEST_CASE("meta - property value write string parse conversion is explicit", "[meta]") {
  REQUIRE(caors::registration::type_<property_conversion_actor>(
              "caors.test.meta.property_conversion_actor")
              .meta()
              .property("value", &property_conversion_actor::value)
              .property("enabled", &property_conversion_actor::enabled)
              .property("scale", &property_conversion_actor::scale)
              .commit() == caors::error{});

  const caors::meta::type reflected =
      require_reflected("caors.test.meta.property_conversion_actor");
  const caors::meta::property value = require_property(reflected, "value");
  const caors::meta::property enabled = require_property(reflected, "enabled");
  const caors::meta::property scale = require_property(reflected, "scale");

  property_conversion_actor instance{};
  caors::meta::value text{caors::core::string{"42"}};
  CHECK_FALSE(value.try_set_value(caors::meta::view(instance), text));
  CHECK(instance.value == 0);

  caors::meta::property_write_options options{};
  options.conversion_policy = caors::meta::argument_conversion_policy::string_parse;
  CHECK(value.try_set_value(caors::meta::view(instance), text, options));
  CHECK(instance.value == 42);

  caors::meta::instance object = caors::meta::view(instance);
  CHECK(object.set("enabled", caors::meta::value{caors::core::string_name{"true"}}, options));
  CHECK(instance.enabled);

  CHECK(object.set("scale", caors::meta::value{caors::core::string{"3.5"}}, options));
  CHECK(instance.scale == 3.5);

  CHECK_FALSE(object.set("value", caors::meta::value{caors::core::string{"not-number"}}, options));
  CHECK(instance.value == 42);
}

TEST_CASE("meta - property value write keeps accessor write access explicit", "[meta]") {
  REQUIRE(caors::registration::type_<failing_setter_property_actor>(
              "caors.test.meta.failing_setter_property_actor")
              .meta()
              .property("value", &failing_setter_property_actor::get_value,
                        &failing_setter_property_actor::set_value)
              .commit() == caors::error{});

  const caors::meta::property value =
      require_property(require_reflected("caors.test.meta.failing_setter_property_actor"), "value");

  failing_setter_property_actor instance{};
  CHECK(value.writable());
  CHECK_FALSE(value.try_set_value(caors::meta::view(instance), caors::meta::value{9}));
  CHECK(instance.value == 3);

  const failing_setter_property_actor& readonly = instance;
  CHECK_FALSE(value.try_set_value(caors::meta::view_const(readonly), caors::meta::value{9}));
  CHECK(instance.value == 3);
}

TEST_CASE("meta - property value write string parse supports named scalars and enum", "[meta]") {
  auto enum_builder = caors::registration::enum_<property_conversion_state>(
      "caors.test.meta.property_conversion_state");
  enum_builder.value("idle", property_conversion_state::idle)
      .value("running", property_conversion_state::running);
  const caors::error enum_result = enum_builder.commit();
  REQUIRE((enum_result == caors::error{} || enum_result == meta_error(caors::errc::duplicate_id)));

  REQUIRE(caors::registration::type_<property_conversion_extended_actor>(
              "caors.test.meta.property_conversion_extended_actor")
              .meta()
              .property("label", &property_conversion_extended_actor::label)
              .property("guid", &property_conversion_extended_actor::guid)
              .property("type", &property_conversion_extended_actor::type)
              .property("state", &property_conversion_extended_actor::state)
              .commit() == caors::error{});

  const caors::meta::type reflected =
      require_reflected("caors.test.meta.property_conversion_extended_actor");
  property_conversion_extended_actor instance{};
  const caors::meta::instance object = caors::meta::view(instance);

  caors::meta::property_write_options options{};
  options.conversion_policy = caors::meta::argument_conversion_policy::string_parse;

  CHECK(object.set("label", caors::meta::value{caors::core::string{"runtime.label"}}, options));
  CHECK(instance.label == "runtime.label");

  CHECK(object.set("guid",
                   caors::meta::value{caors::core::string{"2f6d1f64-7a3a-4db2-9a2d-123456789abc"}},
                   options));
  CHECK(instance.guid.string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");

  CHECK(object.set("type",
                   caors::meta::value{caors::core::string{"5ed39a97-7b20-4d87-86ac-e62a4a615b10"}},
                   options));
  CHECK(instance.type.value.string() == "5ed39a97-7b20-4d87-86ac-e62a4a615b10");

  CHECK(object.set("state", caors::meta::value{caors::core::string{"running"}}, options));
  CHECK(instance.state == property_conversion_state::running);

  CHECK(object.set("state", caors::meta::value{caors::core::string{"1"}}, options));
  CHECK(instance.state == property_conversion_state::idle);

  CHECK_FALSE(object.set("guid", caors::meta::value{caors::core::string{"not-guid"}}, options));
  CHECK(instance.guid.string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");
}

TEST_CASE("meta - serialization field alias does not become property name", "[meta]") {
  REQUIRE(caors::registration::type_<serialized_alias_property_actor>(
              "caors.test.meta.serialized_alias_property_actor")
              .meta()
              .property("value", &serialized_alias_property_actor::value)
              .serialization()
              .field("value", "v")
              .commit() == caors::error{});

  const caors::meta::type reflected =
      require_reflected("caors.test.meta.serialized_alias_property_actor");
  CHECK(reflected.find_property("value"));
  CHECK_FALSE(reflected.find_property("v"));

  const caors::serialization::schema_view schema = caors::serialization::find_schema(reflected);
  REQUIRE(schema);
  REQUIRE(schema.field_count() == 1);
  REQUIRE(schema.field_at(0) != nullptr);
  CHECK(schema.field_at(0)->field_name == "value");
  CHECK(schema.field_at(0)->serialized_name == "v");
}

// 原测试文件：meta_property_property_view_rejects_writes_to_readonly_properties_test.cpp

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
