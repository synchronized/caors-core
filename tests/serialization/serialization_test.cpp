// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace {

struct serializable_actor {
  int id = 0;
  float health = 0.0F;
  bool active = false;
  caors::string name{};
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

[[nodiscard]] bool reflection_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_reflection;
}

[[nodiscard]] bool serialization_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_component;
}

class numeric_field_writer final : public caors::serialization_writer {
public:
  [[nodiscard]] caors::serialization_format_features features() const noexcept override {
    return caors::serialization_format_features::numeric_fields;
  }

  [[nodiscard]] caors::error begin_object(const caors::serialization_type_data&) override {
    object_open = true;
    return {};
  }

  [[nodiscard]] caors::error begin_field(const caors::serialization_field_data& field) override {
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

} // namespace

namespace caors {

template <> struct type_name<unsupported_field_value> {
  static constexpr std::string_view value = "caors.test.serialization.unsupported_value";
};

} // namespace caors

TEST_CASE("serialization - registers type metadata component", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<serializable_actor>("caors.test.serialization.actor")
          .field("id", &serializable_actor::id)
          .field("health", &serializable_actor::health)
          .field("active", &serializable_actor::active)
          .field("name", &serializable_actor::name)
          .commit()));

  const caors::error status = caors::register_serialization<serializable_actor>();

  CHECK(serialization_result_ok(status));
  CHECK(caors::get_serialization_registry().contains(caors::find_type<serializable_actor>()->id()));
  CHECK(caors::get_serialization_registry().size() >= 1);

  const caors::error duplicate = caors::register_serialization<serializable_actor>();
  CHECK(duplicate.failed());
  CHECK(duplicate.domain() == caors::error_domain::serialization);
  CHECK(duplicate.value() == caors::errc::duplicate_component);
}

TEST_CASE("serialization - serializes reflected struct fields to text", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<serializable_actor>("caors.test.serialization.actor")
          .field("id", &serializable_actor::id)
          .field("health", &serializable_actor::health)
          .field("active", &serializable_actor::active)
          .field("name", &serializable_actor::name)
          .commit()));

  const caors::error register_status = caors::register_serialization<serializable_actor>();
  REQUIRE(serialization_result_ok(register_status));

  const serializable_actor actor{7, 95.5F, true, "Ada"};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value == R"({"id":7,"health":95.5,"active":true,"name":"Ada"})");
}

TEST_CASE("serialization - field metadata renames and skips fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<configured_actor>("caors.test.serialization.configured_actor")
          .field("id", &configured_actor::id)
          .field("health", &configured_actor::health)
          .field("debug", &configured_actor::debug)
          .commit()));

  REQUIRE(caors::register_serialization<configured_actor>(
              {
                  caors::make_serialization_field("health", "hp"),
                  caors::make_serialization_field("debug", {}, true),
                  caors::make_serialization_field("id"),
              })
              .succeeded());

  const configured_actor actor{7, 42.5F, true};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value == R"({"hp":42.5,"id":7})");
}

TEST_CASE("serialization - generic writer backend serializes fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<writer_actor>("caors.test.serialization.writer_actor")
          .field("id", &writer_actor::id)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<writer_actor>({
      caors::make_serialization_field("id", {}, false, 7),
  })));

  numeric_field_writer writer;
  const writer_actor actor{42};
  const caors::error result = caors::serialize(actor, writer);

  REQUIRE(result.succeeded());
  CHECK(writer.object_open);
  CHECK(writer.object_closed);
  CHECK(writer.last_field_id == 7);
  CHECK(writer.last_i64 == 42);
}

TEST_CASE("serialization - builds variant from reflected object", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<dom_actor>("caors.test.serialization.dom_actor")
          .field("id", &dom_actor::id)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<dom_actor>({
      caors::make_serialization_field("id", "actor_id", false, 7),
  })));

  const dom_actor actor{42};
  const caors::variant_result result = caors::to_serialization_value(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value.kind() == caors::variant_kind::object);
  REQUIRE(result.value.as_object().size() == 1);
  CHECK(result.value.as_object()[0].name == "actor_id");
  CHECK(result.value.as_object()[0].field_id == 7);
  CHECK(result.value.as_object()[0].value.kind() == caors::variant_kind::signed_integer);
  CHECK(result.value.as_object()[0].value.as_i64() == 42);
}

TEST_CASE("serialization - writes intermediate arrays and objects through text writer",
          "[serialization]") {
  caors::variant::array_type items;
  items.push_back(caors::variant::from(std::int64_t{1}));
  items.push_back(caors::variant::from(std::int64_t{2}));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"items"},
      1,
      caors::variant::array(std::move(items)),
  });

  caors::text_serialization_writer writer;
  const caors::error result = caors::write(writer, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(writer.text() == R"({"items":[1,2]})");
}

TEST_CASE("serialization - variant exposes checked accessors and helpers", "[serialization]") {
  caors::variant items = caors::variant::array({});
  CHECK(items.is_array());
  CHECK(items.try_as_array() != nullptr);
  CHECK(items.try_as_object() == nullptr);
  CHECK(items.push_back(caors::variant::from(std::int64_t{1})));
  CHECK(items.push_back(caors::variant::from(std::int64_t{2})));
  REQUIRE(items.try_as_array() != nullptr);
  REQUIRE(items.try_as_array()->size() == 2);
  CHECK(items.try_as_array()->at(0).as_i64() == 1);

  caors::variant text = caors::variant::from(std::string_view{"Ada"});
  CHECK(text.is_string());
  REQUIRE(text.try_as_string() != nullptr);
  CHECK(*text.try_as_string() == "Ada");
  CHECK(text.try_as_i64() == nullptr);
  CHECK_FALSE(text.push_back(caors::variant::null()));

  caors::variant object = caors::variant::object({});
  CHECK(object.add_member(caors::string_name{"items"}, 7, std::move(items)));
  CHECK(object.find_member(caors::string_name{"items"}) != nullptr);
  CHECK(object.find_member(7) != nullptr);
  CHECK(object.find_member(caors::string_name{"missing"}) == nullptr);
  CHECK(object.find_member(99) == nullptr);
  REQUIRE(object.find_member(7) != nullptr);
  CHECK(object.find_member(7)->value.is_array());
}

TEST_CASE("serialization - converts runtime values to variant", "[serialization]") {
  caors::any number = caors::any::from(42);
  caors::variant_result number_result = caors::to_variant(number);
  REQUIRE(number_result.succeeded());
  CHECK(number_result.value.is_i64());
  REQUIRE(number_result.value.try_as_i64() != nullptr);
  CHECK(*number_result.value.try_as_i64() == 42);

  caors::string name = "Ada";
  caors::any_ref name_ref = caors::any_ref::from(name);
  caors::variant_result name_result = caors::to_variant(name_ref);
  REQUIRE(name_result.succeeded());
  CHECK(name_result.value.is_string());
  REQUIRE(name_result.value.try_as_string() != nullptr);
  CHECK(*name_result.value.try_as_string() == "Ada");

  const std::optional<caors::guid> id = caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  REQUIRE(id.has_value());
  caors::variant_result guid_result = caors::to_variant(caors::any::from(*id));
  REQUIRE(guid_result.succeeded());
  CHECK(guid_result.value.is_string());
  REQUIRE(guid_result.value.try_as_string() != nullptr);
  CHECK(*guid_result.value.try_as_string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");

  caors::variant_result invalid_result = caors::to_variant(caors::any_ref{});
  CHECK(invalid_result.failed());
  CHECK(invalid_result.status.value() == caors::errc::invalid_argument);
}

TEST_CASE("serialization - variant reader deserializes reflected struct fields",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<reader_actor>("caors.test.serialization.reader_actor")
          .field("id", &reader_actor::id)
          .field("health", &reader_actor::health)
          .field("active", &reader_actor::active)
          .field("name", &reader_actor::name)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<reader_actor>()));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{7}),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"health"},
      0,
      caors::variant::from(95.5),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"active"},
      0,
      caors::variant::from(true),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"name"},
      0,
      caors::variant::from(std::string_view{"Ada"}),
  });

  reader_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id == 7);
  CHECK(actor.health == Catch::Approx(95.5));
  CHECK(actor.active);
  CHECK(actor.name == "Ada");
}

TEST_CASE("serialization - variant reader matches fields by stable field id", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<reader_field_id_actor>(
                                   "caors.test.serialization.reader_field_id_actor")
                                   .field("id", &reader_field_id_actor::id)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<reader_field_id_actor>({
      caors::make_serialization_field("id", "actor_id", false, 7),
  })));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"legacy_id"},
      7,
      caors::variant::from(std::int64_t{42}),
  });

  reader_field_id_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id == 42);
}

TEST_CASE("serialization - variant reader reports missing fields", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<reader_missing_field_actor>(
                                   "caors.test.serialization.reader_missing_field_actor")
                                   .field("id", &reader_missing_field_actor::id)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<reader_missing_field_actor>()));

  reader_missing_field_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object({}));

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::not_found);
}

TEST_CASE("serialization - variant reader applies required optional and skipped policies",
          "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<reader_field_policy_actor>(
                                   "caors.test.serialization.reader_field_policy_actor")
                                   .field("id", &reader_field_policy_actor::id)
                                   .field("optional", &reader_field_policy_actor::optional)
                                   .field("required", &reader_field_policy_actor::required)
                                   .field("skipped", &reader_field_policy_actor::skipped)
                                   .commit()));

  caors::serialization_field_data id = caors::make_serialization_field("id");
  caors::serialization_field_data optional = caors::make_serialization_field("optional");
  caors::serialization_field_data required = caors::make_serialization_field("required");
  required.required = true;
  caors::serialization_field_data skipped = caors::make_serialization_field("skipped");
  skipped.skipped = true;

  REQUIRE(serialization_result_ok(caors::register_serialization<reader_field_policy_actor>({
      id,
      optional,
      required,
      skipped,
  })));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{1}),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"required"},
      0,
      caors::variant::from(std::int64_t{3}),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"skipped"},
      0,
      caors::variant::from(std::int64_t{4}),
  });

  reader_field_policy_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id == 1);
  CHECK(actor.optional == 20);
  CHECK(actor.required == 3);
  CHECK(actor.skipped == 40);
}

TEST_CASE("serialization - variant reader reports missing required fields", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<reader_field_policy_actor>(
                                   "caors.test.serialization.reader_field_policy_actor")
                                   .field("id", &reader_field_policy_actor::id)
                                   .field("optional", &reader_field_policy_actor::optional)
                                   .field("required", &reader_field_policy_actor::required)
                                   .field("skipped", &reader_field_policy_actor::skipped)
                                   .commit()));

  caors::serialization_field_data id = caors::make_serialization_field("id");
  caors::serialization_field_data optional = caors::make_serialization_field("optional");
  caors::serialization_field_data required = caors::make_serialization_field("required");
  required.required = true;
  caors::serialization_field_data skipped = caors::make_serialization_field("skipped");
  skipped.skipped = true;

  REQUIRE(serialization_result_ok(caors::register_serialization<reader_field_policy_actor>({
      id,
      optional,
      required,
      skipped,
  })));

  reader_field_policy_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object({}));

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::not_found);
}

TEST_CASE("serialization - variant reader ignores unknown object members", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<reader_unknown_field_actor>(
                                   "caors.test.serialization.reader_unknown_field_actor")
                                   .field("id", &reader_unknown_field_actor::id)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<reader_unknown_field_actor>({
      caors::make_serialization_field("id"),
  })));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{7}),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"extra"},
      0,
      caors::variant::from(std::int64_t{99}),
  });

  reader_unknown_field_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id == 7);
}

TEST_CASE("serialization - context filters fields by schema version", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<versioned_actor>("caors.test.serialization.versioned_actor")
          .field("id", &versioned_actor::id)
          .field("legacy", &versioned_actor::legacy)
          .field("current", &versioned_actor::current)
          .field("future", &versioned_actor::future)
          .commit()));

  caors::serialization_field_data id = caors::make_serialization_field("id");
  caors::serialization_field_data legacy = caors::make_serialization_field("legacy");
  legacy.version_added = 1;
  legacy.version_removed = 2;
  caors::serialization_field_data current = caors::make_serialization_field("current");
  current.version_added = 2;
  caors::serialization_field_data future = caors::make_serialization_field("future");
  future.version_added = 3;

  REQUIRE(serialization_result_ok(caors::register_serialization<versioned_actor>({
      id,
      legacy,
      current,
      future,
  })));

  const versioned_actor actor{1, 10, 20, 30};
  const caors::serialization_text_result text =
      caors::serialize_text(actor, caors::serialization_context{2});

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"id":1,"current":20})");

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{2}),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"current"},
      0,
      caors::variant::from(std::int64_t{22}),
  });

  versioned_actor deserialized{};
  const caors::error result = caors::deserialize(
      deserialized, caors::variant::object(std::move(members)), caors::serialization_context{2});

  REQUIRE(result.succeeded());
  CHECK(deserialized.id == 2);
  CHECK(deserialized.legacy == 0);
  CHECK(deserialized.current == 22);
  CHECK(deserialized.future == 0);
}

TEST_CASE("serialization - built in codecs serialize guid and type guid as scalar strings",
          "[serialization]") {
  const std::optional<caors::guid> id = caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  const std::optional<caors::guid> type =
      caors::guid::parse("5ed39a97-7b20-4d87-86ac-e62a4a615b10");
  REQUIRE(id.has_value());
  REQUIRE(type.has_value());

  REQUIRE(reflection_result_ok(
      caors::reflection::class_<codec_guid_actor>("caors.test.serialization.codec_guid_actor")
          .field("id", &codec_guid_actor::id)
          .field("type", &codec_guid_actor::type)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<codec_guid_actor>()));

  const codec_guid_actor actor{*id, caors::type_guid{*type}};
  const caors::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(
      text.value ==
      R"({"id":"2f6d1f64-7a3a-4db2-9a2d-123456789abc","type":"5ed39a97-7b20-4d87-86ac-e62a4a615b10"})");

  const caors::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.as_object().size() == 2);
  CHECK(value.value.as_object()[0].value.kind() == caors::variant_kind::string);
  CHECK(value.value.as_object()[0].value.as_string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  CHECK(value.value.as_object()[1].value.kind() == caors::variant_kind::string);
  CHECK(value.value.as_object()[1].value.as_string() == "5ed39a97-7b20-4d87-86ac-e62a4a615b10");
}

TEST_CASE("serialization - built in codecs deserialize guid and type guid from strings",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<codec_guid_actor>("caors.test.serialization.codec_guid_actor")
          .field("id", &codec_guid_actor::id)
          .field("type", &codec_guid_actor::type)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<codec_guid_actor>()));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::string_view{"2f6d1f64-7a3a-4db2-9a2d-123456789abc"}),
  });
  members.push_back(caors::variant_member{
      caors::string_name{"type"},
      0,
      caors::variant::from(std::string_view{"5ed39a97-7b20-4d87-86ac-e62a4a615b10"}),
  });

  codec_guid_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id.string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  CHECK(actor.type.value.string() == "5ed39a97-7b20-4d87-86ac-e62a4a615b10");
}

TEST_CASE("serialization - built in codecs reject invalid guid text", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<codec_guid_actor>("caors.test.serialization.codec_guid_actor")
          .field("id", &codec_guid_actor::id)
          .field("type", &codec_guid_actor::type)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<codec_guid_actor>()));

  caors::variant::object_type members;
  members.push_back(caors::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::string_view{"invalid-guid"}),
  });

  codec_guid_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_argument);
}

TEST_CASE("serialization - built in codecs serialize string view fields", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<codec_string_view_actor>(
                                   "caors.test.serialization.codec_string_view_actor")
                                   .field("name", &codec_string_view_actor::name)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<codec_string_view_actor>()));

  const codec_string_view_actor actor{std::string_view{"Ada"}};
  const caors::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"name":"Ada"})");
}

TEST_CASE("serialization - writer features validate required field identifiers",
          "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<numeric_writer_actor>(
                                   "caors.test.serialization.numeric_writer_actor")
                                   .field("id", &numeric_writer_actor::id)
                                   .commit()));
  REQUIRE(
      caors::register_serialization<numeric_writer_actor>({
                                                              caors::make_serialization_field("id"),
                                                          })
          .succeeded());

  numeric_field_writer writer;
  const numeric_writer_actor actor{5};
  const caors::error result = caors::serialize(actor, writer);

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_field);
}

TEST_CASE("serialization - registration can happen before reflection fields", "[serialization]") {
  REQUIRE(caors::register_serialization<serialization_first_actor>(
              {
                  caors::make_serialization_field("id", "actor_id"),
              })
              .succeeded());

  REQUIRE(reflection_result_ok(caors::reflection::class_<serialization_first_actor>(
                                   "caors.test.serialization.serialization_first_actor")
                                   .field("id", &serialization_first_actor::id)
                                   .commit()));

  const serialization_first_actor actor{9};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value == R"({"actor_id":9})");
}

TEST_CASE("serialization - reports missing serialization metadata", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<reflected_only_actor>("caors.test.serialization.reflected_only")
          .field("id", &reflected_only_actor::id)
          .commit()));

  const reflected_only_actor actor{3};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::missing_type);
  CHECK(result.value.empty());
}

TEST_CASE("serialization - reports missing configured reflection field", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<missing_field_actor>("caors.test.serialization.missing_field")
          .field("id", &missing_field_actor::id)
          .commit()));
  REQUIRE(caors::register_serialization<missing_field_actor>(
              {
                  caors::make_serialization_field("missing"),
              })
              .succeeded());

  const missing_field_actor actor{3};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::not_found);
}

TEST_CASE("serialization - reports unsupported reflected field kind", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<unsupported_field_actor>(
                                   "caors.test.serialization.unsupported_field")
                                   .field("value", &unsupported_field_actor::value)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<unsupported_field_actor>()));

  const unsupported_field_actor actor{{1}};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::unsupported);
}
