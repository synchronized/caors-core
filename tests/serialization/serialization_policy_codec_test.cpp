// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - variant reader matches fields by stable field id", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<reader_field_id_actor>(
                                   "caors.test.serialization.reader_field_id_actor")
                                   .field("id", &reader_field_id_actor::id)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<reader_field_id_actor>({
      caors::serialization::make_field("id", "actor_id", false, 7),
  })));

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
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
  REQUIRE(
      serialization_result_ok(caors::serialization::register_type<reader_missing_field_actor>()));

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

  caors::serialization::field_data id = caors::serialization::make_field("id");
  caors::serialization::field_data optional = caors::serialization::make_field("optional");
  caors::serialization::field_data required = caors::serialization::make_field("required");
  required.required = true;
  caors::serialization::field_data skipped = caors::serialization::make_field("skipped");
  skipped.skipped = true;

  REQUIRE(serialization_result_ok(caors::serialization::register_type<reader_field_policy_actor>({
      id,
      optional,
      required,
      skipped,
  })));

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{1}),
  });
  members.push_back(caors::value::variant_member{
      caors::string_name{"required"},
      0,
      caors::variant::from(std::int64_t{3}),
  });
  members.push_back(caors::value::variant_member{
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

  caors::serialization::field_data id = caors::serialization::make_field("id");
  caors::serialization::field_data optional = caors::serialization::make_field("optional");
  caors::serialization::field_data required = caors::serialization::make_field("required");
  required.required = true;
  caors::serialization::field_data skipped = caors::serialization::make_field("skipped");
  skipped.skipped = true;

  REQUIRE(serialization_result_ok(caors::serialization::register_type<reader_field_policy_actor>({
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
  REQUIRE(serialization_result_ok(caors::serialization::register_type<reader_unknown_field_actor>({
      caors::serialization::make_field("id"),
  })));

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{7}),
  });
  members.push_back(caors::value::variant_member{
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

  caors::serialization::field_data id = caors::serialization::make_field("id");
  caors::serialization::field_data legacy = caors::serialization::make_field("legacy");
  legacy.version_added = 1;
  legacy.version_removed = 2;
  caors::serialization::field_data current = caors::serialization::make_field("current");
  current.version_added = 2;
  caors::serialization::field_data future = caors::serialization::make_field("future");
  future.version_added = 3;
  future.required = true;

  REQUIRE(serialization_result_ok(caors::serialization::register_type<versioned_actor>({
      id,
      legacy,
      current,
      future,
  })));

  const versioned_actor actor{1, 10, 20, 30};
  const caors::serialization::serialization_text_result text_v2 =
      caors::serialize_text(actor, caors::serialization::context{2});

  REQUIRE(text_v2.succeeded());
  CHECK(text_v2.value == R"({"id":1,"current":20})");

  const caors::serialization::serialization_text_result text_v3 =
      caors::serialize_text(actor, caors::serialization::context{3});

  REQUIRE(text_v3.succeeded());
  CHECK(text_v3.value == R"({"id":1,"current":20,"future":30})");

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{2}),
  });
  members.push_back(caors::value::variant_member{
      caors::string_name{"current"},
      0,
      caors::variant::from(std::int64_t{22}),
  });

  versioned_actor deserialized{};
  const caors::error result = caors::deserialize(
      deserialized, caors::variant::object(std::move(members)), caors::serialization::context{2});

  REQUIRE(result.succeeded());
  CHECK(deserialized.id == 2);
  CHECK(deserialized.legacy == 0);
  CHECK(deserialized.current == 22);
  CHECK(deserialized.future == 0);

  caors::variant::object_type extra_members;
  extra_members.push_back(caors::value::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{3}),
  });
  extra_members.push_back(caors::value::variant_member{
      caors::string_name{"legacy"},
      0,
      caors::variant::from(std::int64_t{33}),
  });
  extra_members.push_back(caors::value::variant_member{
      caors::string_name{"current"},
      0,
      caors::variant::from(std::int64_t{44}),
  });
  extra_members.push_back(caors::value::variant_member{
      caors::string_name{"future"},
      0,
      caors::variant::from(std::int64_t{55}),
  });

  versioned_actor filtered{0, 10, 0, 30};
  const caors::error filtered_result = caors::deserialize(
      filtered, caors::variant::object(std::move(extra_members)), caors::serialization::context{2});

  REQUIRE(filtered_result.succeeded());
  CHECK(filtered.id == 3);
  CHECK(filtered.legacy == 10);
  CHECK(filtered.current == 44);
  CHECK(filtered.future == 30);

  caors::variant::object_type missing_required_members;
  missing_required_members.push_back(caors::value::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{4}),
  });
  missing_required_members.push_back(caors::value::variant_member{
      caors::string_name{"current"},
      0,
      caors::variant::from(std::int64_t{55}),
  });

  versioned_actor missing_required{};
  const caors::error missing_required_result = caors::deserialize(
      missing_required, caors::variant::object(std::move(missing_required_members)),
      caors::serialization::context{3});

  CHECK(missing_required_result.failed());
  CHECK(missing_required_result.domain() == caors::error_domain::serialization);
  CHECK(missing_required_result.value() == caors::errc::not_found);
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
  caors::string backing;
  caors::serialization::context context{};
  context.string_view_storage = &backing;

  const caors::error result = caors::deserialize_json(actor, R"({"name":"Ada"})", context);

  REQUIRE(result.succeeded());
  CHECK(backing == "Ada");
  CHECK(actor.name == "Ada");
  CHECK(actor.name.data() == backing.data());
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

TEST_CASE("serialization - writer features validate required field identifiers",
          "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<numeric_writer_actor>(
                                   "caors.test.serialization.numeric_writer_actor")
                                   .field("id", &numeric_writer_actor::id)
                                   .commit()));
  REQUIRE(caors::serialization::register_type<numeric_writer_actor>(
              {
                  caors::serialization::make_field("id"),
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
  const caors::meta::type type = caors::meta::type_<serialization_first_actor>(
                                          "caors.test.serialization.serialization_first_actor")
                                          .get();
  REQUIRE(type.valid());

  REQUIRE(caors::serialization::register_type<serialization_first_actor>(
              {
                  caors::serialization::make_field("id", "actor_id"),
              })
              .succeeded());

  REQUIRE(reflection_result_ok(caors::reflection::class_<serialization_first_actor>(
                                   "caors.test.serialization.serialization_first_actor")
                                   .field("id", &serialization_first_actor::id)
                                   .commit()));

  const serialization_first_actor actor{9};
  const caors::serialization::serialization_text_result result = caors::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value == R"({"actor_id":9})");
}

TEST_CASE("serialization - reports missing serialization metadata", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<reflected_only_actor>("caors.test.serialization.reflected_only")
          .field("id", &reflected_only_actor::id)
          .commit()));

  const reflected_only_actor actor{3};
  const caors::serialization::serialization_text_result result = caors::serialize_text(actor);

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
  REQUIRE(caors::serialization::register_type<missing_field_actor>(
              {
                  caors::serialization::make_field("missing"),
              })
              .succeeded());

  const missing_field_actor actor{3};
  const caors::serialization::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::not_found);
}

TEST_CASE("serialization - reports unsupported reflected field kind", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<unsupported_field_actor>(
                                   "caors.test.serialization.unsupported_field")
                                   .field("value", &unsupported_field_actor::value)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<unsupported_field_actor>()));

  const unsupported_field_actor actor{{1}};
  const caors::serialization::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::unsupported);
}

TEST_CASE("serialization - reports unsupported weak pointer fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<weak_ptr_actor>("caors.test.serialization.weak_ptr_actor")
          .field("value", &weak_ptr_actor::value, "std.weak_ptr<int>")
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<weak_ptr_actor>()));

  const std::shared_ptr<int> value = std::make_shared<int>(7);
  const weak_ptr_actor actor{value};
  const caors::serialization::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::unsupported);
}

TEST_CASE("serialization - rejects weak pointer deserialization", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<weak_ptr_actor>("caors.test.serialization.weak_ptr_actor")
          .field("value", &weak_ptr_actor::value, "std.weak_ptr<int>")
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<weak_ptr_actor>()));

  weak_ptr_actor actor{};
  const caors::error result = caors::deserialize_json(actor, R"({"value":7})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
  CHECK(actor.value.expired());
}
