// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - registers type metadata component", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<serializable_actor>("caors.test.serialization.actor")
          .field("id", &serializable_actor::id)
          .field("health", &serializable_actor::health)
          .field("active", &serializable_actor::active)
          .field("name", &serializable_actor::name)
          .commit()));

  const caors::error status = caors::serialization::register_type<serializable_actor>();

  CHECK(serialization_result_ok(status));
  CHECK(caors::serialization::contains(caors::meta::find<serializable_actor>().id()));
  CHECK(caors::serialization::size() >= 1);

  const caors::error duplicate = caors::serialization::register_type<serializable_actor>();
  CHECK(duplicate.failed());
  CHECK(duplicate.domain() == caors::error_domain::serialization);
  CHECK(duplicate.value() == caors::errc::duplicate_component);
}

TEST_CASE("serialization - class builder can attach to explicit meta type", "[serialization]") {
  const caors::meta::type type = caors::meta::type_<explicit_meta_serialization_actor>(
                                     "caors.test.serialization.explicit_meta_actor")
                                     .get();
  REQUIRE(type.valid());
  CHECK(caors::meta::find<explicit_meta_serialization_actor>().id() == type.id());

  REQUIRE(reflection_result_ok(caors::reflection::class_<explicit_meta_serialization_actor>(type)
                                   .field("id", &explicit_meta_serialization_actor::id)
                                   .commit()));

  const caors::error status = caors::serialization::class_(type).field("id").commit();
  REQUIRE(serialization_result_ok(status));

  const explicit_meta_serialization_actor actor{42};
  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"id":42})");
}

TEST_CASE("serialization - explicit registration reports missing meta type", "[serialization]") {
  const caors::error status = caors::serialization::register_type(
      caors::type_id{std::numeric_limits<std::uint64_t>::max()});

  CHECK(status.failed());
  CHECK(status.domain() == caors::error_domain::serialization);
  CHECK(status.value() == caors::errc::missing_type);
}

TEST_CASE("serialization - serializes reflected struct fields to text", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<serializable_actor>("caors.test.serialization.actor")
          .field("id", &serializable_actor::id)
          .field("health", &serializable_actor::health)
          .field("active", &serializable_actor::active)
          .field("name", &serializable_actor::name)
          .commit()));

  const caors::error register_status = caors::serialization::register_type<serializable_actor>();
  REQUIRE(serialization_result_ok(register_status));

  const serializable_actor actor{7, 95.5F, true, "Ada"};
  const caors::serialization::serialization_text_result result = caors::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value == R"({"id":7,"health":95.5,"active":true,"name":"Ada"})");
}

TEST_CASE("serialization - serializes reflected struct fields to json", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<serializable_actor>("caors.test.serialization.actor")
          .field("id", &serializable_actor::id)
          .field("health", &serializable_actor::health)
          .field("active", &serializable_actor::active)
          .field("name", &serializable_actor::name)
          .commit()));

  const caors::error register_status = caors::serialization::register_type<serializable_actor>();
  REQUIRE(serialization_result_ok(register_status));

  const serializable_actor actor{7, 95.5F, true, "Ada\nLovelace"};
  const caors::serialization::serialization_text_result result = caors::serialize_json(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value == R"({"id":7,"health":95.5,"active":true,"name":"Ada\nLovelace"})");
}

TEST_CASE("serialization - field metadata renames and skips fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<configured_actor>("caors.test.serialization.configured_actor")
          .field("id", &configured_actor::id)
          .field("health", &configured_actor::health)
          .field("debug", &configured_actor::debug)
          .commit()));

  REQUIRE(caors::serialization::register_type<configured_actor>(
              {
                  caors::serialization::make_field("health", "hp"),
                  caors::serialization::make_field("debug", {}, true),
                  caors::serialization::make_field("id"),
              })
              .succeeded());

  const configured_actor actor{7, 42.5F, true};
  const caors::serialization::serialization_text_result result = caors::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value == R"({"hp":42.5,"id":7})");
}

TEST_CASE("serialization - generic writer backend serializes fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<writer_actor>("caors.test.serialization.writer_actor")
          .field("id", &writer_actor::id)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<writer_actor>({
      caors::serialization::make_field("id", {}, false, 7),
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
  REQUIRE(serialization_result_ok(caors::serialization::register_type<dom_actor>({
      caors::serialization::make_field("id", "actor_id", false, 7),
  })));

  const dom_actor actor{42};
  const caors::serialization::variant_result result = caors::to_serialization_value(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value.kind() == caors::value::variant_kind::object);
  REQUIRE(result.value.as_object().size() == 1);
  CHECK(result.value.as_object()[0].name == "actor_id");
  CHECK(result.value.as_object()[0].field_id == 7);
  CHECK(result.value.as_object()[0].value.kind() == caors::value::variant_kind::signed_integer);
  CHECK(result.value.as_object()[0].value.as_i64() == 42);
}

TEST_CASE("serialization - writes intermediate arrays and objects through text writer",
          "[serialization]") {
  caors::variant::array_type items;
  items.push_back(caors::variant::from(std::int64_t{1}));
  items.push_back(caors::variant::from(std::int64_t{2}));

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"items"},
      1,
      caors::variant::array(std::move(items)),
  });

  caors::serialization::text_writer writer;
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
  require_registered_type<int>("int");
  require_registered_type<caors::string>("caors.string");
  require_registered_type<caors::guid>("caors.guid");

  caors::any number = caors::any::from(42);
  caors::serialization::variant_result number_result = caors::serialization::to_variant(number);
  REQUIRE(number_result.succeeded());
  CHECK(number_result.value.is_i64());
  REQUIRE(number_result.value.try_as_i64() != nullptr);
  CHECK(*number_result.value.try_as_i64() == 42);

  caors::string name = "Ada";
  caors::any_ref name_ref = caors::any_ref::from(name);
  caors::serialization::variant_result name_result = caors::serialization::to_variant(name_ref);
  REQUIRE(name_result.succeeded());
  CHECK(name_result.value.is_string());
  REQUIRE(name_result.value.try_as_string() != nullptr);
  CHECK(*name_result.value.try_as_string() == "Ada");

  const std::optional<caors::guid> id = caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  REQUIRE(id.has_value());
  caors::serialization::variant_result guid_result =
      caors::serialization::to_variant(caors::any::from(*id));
  REQUIRE(guid_result.succeeded());
  CHECK(guid_result.value.is_string());
  REQUIRE(guid_result.value.try_as_string() != nullptr);
  CHECK(*guid_result.value.try_as_string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");

  caors::serialization::variant_result invalid_result =
      caors::serialization::to_variant(caors::any_ref{});
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
  REQUIRE(serialization_result_ok(caors::serialization::register_type<reader_actor>()));

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{7}),
  });
  members.push_back(caors::value::variant_member{
      caors::string_name{"health"},
      0,
      caors::variant::from(95.5),
  });
  members.push_back(caors::value::variant_member{
      caors::string_name{"active"},
      0,
      caors::variant::from(true),
  });
  members.push_back(caors::value::variant_member{
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

TEST_CASE("serialization - parses json and deserializes reflected struct fields",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<reader_actor>("caors.test.serialization.reader_actor")
          .field("id", &reader_actor::id)
          .field("health", &reader_actor::health)
          .field("active", &reader_actor::active)
          .field("name", &reader_actor::name)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<reader_actor>()));

  const caors::serialization::variant_result parsed =
      caors::parse_json(R"({"id":7,"health":95.5,"active":true,"name":"Ada\nLovelace"})");
  REQUIRE(parsed.succeeded());
  REQUIRE(parsed.value.is_object());
  const caors::value::variant_member* name = parsed.value.find_member(caors::string_name{"name"});
  REQUIRE(name != nullptr);
  REQUIRE(name->value.is_string());
  CHECK(name->value.as_string() == "Ada\nLovelace");

  reader_actor actor{};
  const caors::error result =
      caors::deserialize_json(actor, R"({"id":7,"health":95.5,"active":true,"name":"Ada"})");

  REQUIRE(result.succeeded());
  CHECK(actor.id == 7);
  CHECK(actor.health == Catch::Approx(95.5));
  CHECK(actor.active);
  CHECK(actor.name == "Ada");
}

TEST_CASE("serialization - roundtrips char and enum fields", "[serialization]") {
  require_scalar_actor_metadata();

  scalar_actor actor{};
  actor.code = 'A';
  actor.color = scalar_color::blue;
  actor.state = scalar_state::enabled;

  const caors::serialization::serialization_text_result text = caors::serialize_json(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"code":"A","color":2,"state":2})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());

  const caors::value::variant_member* code = value.value.find_member(caors::string_name{"code"});
  const caors::value::variant_member* color = value.value.find_member(caors::string_name{"color"});
  const caors::value::variant_member* state = value.value.find_member(caors::string_name{"state"});
  REQUIRE(code != nullptr);
  REQUIRE(color != nullptr);
  REQUIRE(state != nullptr);
  REQUIRE(code->value.try_as_string() != nullptr);
  REQUIRE(color->value.try_as_u64() != nullptr);
  REQUIRE(state->value.try_as_i64() != nullptr);
  CHECK(code->value.as_string() == "A");
  CHECK(color->value.as_u64() == 2U);
  CHECK(state->value.as_i64() == 2);

  scalar_actor decoded{};
  const caors::error result =
      caors::deserialize_json(decoded, R"({"code":"Z","color":1,"state":-1})");
  REQUIRE(result.succeeded());
  CHECK(decoded.code == 'Z');
  CHECK(decoded.color == scalar_color::red);
  CHECK(decoded.state == scalar_state::disabled);
}

TEST_CASE("serialization - rejects invalid char and enum values", "[serialization]") {
  require_scalar_actor_metadata();

  scalar_actor actor{};

  const caors::error invalid_char =
      caors::deserialize_json(actor, R"({"code":"AB","color":1,"state":-1})");
  REQUIRE(invalid_char.failed());
  CHECK(invalid_char.domain() == caors::error_domain::serialization);

  const caors::error invalid_enum =
      caors::deserialize_json(actor, R"({"code":"A","color":300,"state":-1})");
  REQUIRE(invalid_enum.failed());
  CHECK(invalid_enum.domain() == caors::error_domain::serialization);
  CHECK(invalid_enum.value() == caors::errc::invalid_argument);
}

TEST_CASE("serialization - rejects invalid json", "[serialization]") {
  const caors::serialization::variant_result parsed = caors::parse_json(R"({"id":1,})");

  CHECK(parsed.failed());
  CHECK(parsed.status.domain() == caors::error_domain::serialization);
  CHECK(parsed.status.value() == caors::errc::invalid_argument);
}
