// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

namespace {

struct late_builtin_codec_actor {
  std::chrono::seconds timeout{};
};

} // namespace

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
  REQUIRE(serialization_result_ok(caors::serialization::register_type<codec_guid_actor>()));

  const codec_guid_actor actor{*id, caors::type_guid{*type}};
  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(
      text.value ==
      R"({"id":"2f6d1f64-7a3a-4db2-9a2d-123456789abc","type":"5ed39a97-7b20-4d87-86ac-e62a4a615b10"})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.as_object().size() == 2);
  CHECK(value.value.as_object()[0].value.kind() == caors::value::variant_kind::string);
  CHECK(value.value.as_object()[0].value.as_string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  CHECK(value.value.as_object()[1].value.kind() == caors::value::variant_kind::string);
  CHECK(value.value.as_object()[1].value.as_string() == "5ed39a97-7b20-4d87-86ac-e62a4a615b10");
}

TEST_CASE("serialization - built in codecs deserialize guid and type guid from strings",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<codec_guid_actor>("caors.test.serialization.codec_guid_actor")
          .field("id", &codec_guid_actor::id)
          .field("type", &codec_guid_actor::type)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<codec_guid_actor>()));

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::string_view{"2f6d1f64-7a3a-4db2-9a2d-123456789abc"}),
  });
  members.push_back(caors::value::variant_member{
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
  REQUIRE(serialization_result_ok(caors::serialization::register_type<codec_guid_actor>()));

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
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
  REQUIRE(serialization_result_ok(caors::serialization::register_type<codec_string_view_actor>()));

  const codec_string_view_actor actor{std::string_view{"Ada"}};
  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"name":"Ada"})");
}

TEST_CASE("serialization - built in codecs reject string view deserialization", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<codec_string_view_actor>(
                                   "caors.test.serialization.codec_string_view_actor")
                                   .field("name", &codec_string_view_actor::name)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<codec_string_view_actor>()));

  codec_string_view_actor actor{std::string_view{"original"}};
  const caors::error result = caors::deserialize_json(actor, R"({"name":"Ada"})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
  CHECK(actor.name == "original");
}

TEST_CASE("serialization - string view deserialization can use explicit backing storage",
          "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<codec_string_view_actor>(
                                   "caors.test.serialization.codec_string_view_actor")
                                   .field("name", &codec_string_view_actor::name)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<codec_string_view_actor>()));

  codec_string_view_actor actor{std::string_view{"original"}};
  caors::serialization::string_storage storage;
  caors::serialization::context context{};
  context.string_view_storage = &storage;

  const caors::error result = caors::deserialize_json(actor, R"({"name":"Ada"})", context);

  REQUIRE(result.succeeded());
  REQUIRE(storage.size() == 1);
  CHECK(actor.name == "Ada");
}

TEST_CASE("serialization - string view storage keeps multiple deserialized strings",
          "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<codec_string_view_pair_actor>(
                                   "caors.test.serialization.codec_string_view_pair_actor")
                                   .field("first", &codec_string_view_pair_actor::first)
                                   .field("second", &codec_string_view_pair_actor::second)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::register_type<codec_string_view_pair_actor>()));

  codec_string_view_pair_actor actor{};
  caors::serialization::string_storage storage;
  caors::serialization::context context{};
  context.string_view_storage = &storage;

  const caors::error result =
      caors::deserialize_json(actor, R"({"first":"Ada","second":"Mina"})", context);

  REQUIRE(result.succeeded());
  CHECK(actor.first == "Ada");
  CHECK(actor.second == "Mina");
  CHECK(actor.first.data() != actor.second.data());
  CHECK(storage.size() == 2);
}

TEST_CASE("serialization - built in codecs roundtrip filesystem paths as strings",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<codec_path_actor>("caors.test.serialization.codec_path_actor")
          .field("path", &codec_path_actor::path)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<codec_path_actor>()));

  codec_path_actor actor{std::filesystem::path{"assets/textures/albedo.png"}};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"path":"assets/textures/albedo.png"})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* path = value.value.find_member(caors::string_name{"path"});
  REQUIRE(path != nullptr);
  REQUIRE(path->value.try_as_string() != nullptr);
  CHECK(*path->value.try_as_string() == "assets/textures/albedo.png");

  codec_path_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"path":"levels/start.scene"})");

  REQUIRE(result.succeeded());
  CHECK(decoded.path.generic_string() == "levels/start.scene");
}

TEST_CASE("serialization - built in codecs roundtrip chrono values as integers",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<codec_chrono_actor>("caors.test.serialization.codec_chrono_actor")
          .field("timeout", &codec_chrono_actor::timeout)
          .field("timestamp", &codec_chrono_actor::timestamp)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<codec_chrono_actor>()));

  codec_chrono_actor actor{
      std::chrono::milliseconds{2500},
      std::chrono::system_clock::time_point{std::chrono::milliseconds{1700000000123}},
  };

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"timeout":2500,"timestamp":1700000000123})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* timeout =
      value.value.find_member(caors::string_name{"timeout"});
  const caors::value::variant_member* timestamp =
      value.value.find_member(caors::string_name{"timestamp"});
  REQUIRE(timeout != nullptr);
  REQUIRE(timestamp != nullptr);
  REQUIRE(timeout->value.try_as_i64() != nullptr);
  REQUIRE(timestamp->value.try_as_i64() != nullptr);
  CHECK(*timeout->value.try_as_i64() == 2500);
  CHECK(*timestamp->value.try_as_i64() == 1700000000123);

  codec_chrono_actor decoded{};
  const caors::error result =
      caors::deserialize_json(decoded, R"({"timeout":125,"timestamp":1700000000456})");

  REQUIRE(result.succeeded());
  CHECK(decoded.timeout == std::chrono::milliseconds{125});
  CHECK(decoded.timestamp.time_since_epoch() == std::chrono::milliseconds{1700000000456});
}

TEST_CASE("serialization - built in codecs survive late meta type registration",
          "[serialization]") {
  const std::optional<caors::guid> id = caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  REQUIRE(id.has_value());

  const caors::serialization::variant_result probe =
      caors::serialization::to_variant(caors::any::from(*id));
  REQUIRE(probe.succeeded());
  CHECK_FALSE(caors::meta::find<std::chrono::seconds>());

  require_registered_type<std::chrono::seconds>("std.chrono.seconds");
  REQUIRE(reflection_result_ok(caors::reflection::class_<late_builtin_codec_actor>(
                                   "caors.test.serialization.late_builtin_codec_actor")
                                   .field("timeout", &late_builtin_codec_actor::timeout)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<late_builtin_codec_actor>()));

  const late_builtin_codec_actor actor{std::chrono::seconds{42}};
  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"timeout":42})");

  late_builtin_codec_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"timeout":7})");

  REQUIRE(result.succeeded());
  CHECK(decoded.timeout == std::chrono::seconds{7});
}
