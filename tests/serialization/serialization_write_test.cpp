// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

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

TEST_CASE("serialization - serializes json to caller provided buffer", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<serializable_actor>("caors.test.serialization.actor")
          .field("id", &serializable_actor::id)
          .field("health", &serializable_actor::health)
          .field("active", &serializable_actor::active)
          .field("name", &serializable_actor::name)
          .commit()));

  const caors::error register_status = caors::serialization::register_type<serializable_actor>();
  REQUIRE(serialization_result_ok(register_status));

  const serializable_actor actor{3, 10.5F, false, "Lin"};
  caors::string output = "previous";
  const caors::error status = caors::serialize_json_to(actor, output);

  REQUIRE(status.succeeded());
  CHECK(output == R"({"id":3,"health":10.5,"active":false,"name":"Lin"})");

  output = "previous";
  const caors::error text_status = caors::serialize_text_to(actor, output);
  REQUIRE(text_status.succeeded());
  CHECK(output == R"({"id":3,"health":10.5,"active":false,"name":"Lin"})");
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
  const caors::error result =
      caors::serialization::write_variant(writer, caors::variant::object(std::move(members)));

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
