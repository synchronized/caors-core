// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_read_parses_json_and_deserializes_reflected_struct_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - parses json and deserializes reflected struct fields",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::class_<reader_actor>("caors.test.serialization.reader_actor"),
       caors::reflection::class_<reader_actor>())
          .field("id", &reader_actor::id)
          .field("health", &reader_actor::health)
          .field("active", &reader_actor::active)
          .field("name", &reader_actor::name)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<reader_actor>().commit()));

  const caors::serialization::variant_result parsed = caors::serialization::parse_json(
      R"({"id":7,"health":95.5,"active":true,"name":"Ada\nLovelace"})");
  REQUIRE(parsed.succeeded());
  REQUIRE(parsed.value.is_object());
  const caors::value::variant_member* name = parsed.value.find_member(caors::string_name{"name"});
  REQUIRE(name != nullptr);
  REQUIRE(name->value.is_string());
  CHECK(name->value.as_string() == "Ada\nLovelace");

  reader_actor actor{};
  const caors::error result =
      caors::deserialize(actor, R"({"id":7,"health":95.5,"active":true,"name":"Ada"})");

  REQUIRE(result.succeeded());
  CHECK(actor.id == 7);
  CHECK(actor.health == Catch::Approx(95.5));
  CHECK(actor.active);
  CHECK(actor.name == "Ada");
}

// 原测试文件：serialization_read_rejects_invalid_char_and_enum_values_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects invalid char and enum values", "[serialization]") {
  require_scalar_actor_metadata();

  scalar_actor actor{};

  const caors::error invalid_char =
      caors::deserialize(actor, R"({"code":"AB","color":1,"state":-1})");
  REQUIRE(invalid_char.failed());
  CHECK(invalid_char.domain() == caors::error_domain::serialization);

  const caors::error invalid_enum =
      caors::deserialize(actor, R"({"code":"A","color":300,"state":-1})");
  REQUIRE(invalid_enum.failed());
  CHECK(invalid_enum.domain() == caors::error_domain::serialization);
  CHECK(invalid_enum.value() == caors::errc::invalid_argument);
}

// 原测试文件：serialization_read_rejects_invalid_json_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects invalid json", "[serialization]") {
  const caors::serialization::variant_result parsed =
      caors::serialization::parse_json(R"({"id":1,})");

  CHECK(parsed.failed());
  CHECK(parsed.status.domain() == caors::error_domain::serialization);
  CHECK(parsed.status.value() == caors::errc::invalid_argument);
}

// 原测试文件：serialization_read_roundtrips_char_and_enum_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips char and enum fields", "[serialization]") {
  require_scalar_actor_metadata();

  scalar_actor actor{};
  actor.code = 'A';
  actor.color = scalar_color::blue;
  actor.state = scalar_state::enabled;

  const caors::serialization::serialization_text_result text = caors::to_string(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"code":"A","color":2,"state":2})");

  const caors::serialization::variant_result value = caors::to_value(actor);
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
  const caors::error result = caors::deserialize(decoded, R"({"code":"Z","color":1,"state":-1})");
  REQUIRE(result.succeeded());
  CHECK(decoded.code == 'Z');
  CHECK(decoded.color == scalar_color::red);
  CHECK(decoded.state == scalar_state::disabled);
}

// 原测试文件：serialization_read_variant_reader_deserializes_reflected_struct_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - variant reader deserializes reflected struct fields",
          "[serialization]") {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::class_<reader_actor>("caors.test.serialization.reader_actor"),
       caors::reflection::class_<reader_actor>())
          .field("id", &reader_actor::id)
          .field("health", &reader_actor::health)
          .field("active", &reader_actor::active)
          .field("name", &reader_actor::name)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<reader_actor>().commit()));

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

// 原测试文件：serialization_reader_context_filters_fields_by_schema_version_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - context filters fields by schema version", "[serialization]") {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::class_<versioned_actor>("caors.test.serialization.versioned_actor"),
       caors::reflection::class_<versioned_actor>())
          .field("id", &versioned_actor::id)
          .field("legacy", &versioned_actor::legacy)
          .field("current", &versioned_actor::current)
          .field("future", &versioned_actor::future)
          .commit()));

  caors::serialization::field_schema id = caors::serialization::make_field("id");
  caors::serialization::field_schema legacy = caors::serialization::make_field("legacy");
  legacy.version_added = 1;
  legacy.version_removed = 2;
  caors::serialization::field_schema current = caors::serialization::make_field("current");
  current.version_added = 2;
  caors::serialization::field_schema future = caors::serialization::make_field("future");
  future.version_added = 3;
  future.required = true;

  REQUIRE(serialization_result_ok(caors::serialization::class_<versioned_actor>({
                                                                                    id,
                                                                                    legacy,
                                                                                    current,
                                                                                    future,
                                                                                })
                                      .commit()));

  const versioned_actor actor{1, 10, 20, 30};
  caors::serialization::context context_v2{};
  context_v2.config.fields.schema_version = 2;
  const caors::serialization::serialization_text_result text_v2 =
      caors::serialization::serialize_text(context_v2, actor);

  REQUIRE(text_v2.succeeded());
  CHECK(text_v2.value == R"({"id":1,"current":20})");

  caors::serialization::context context_v3{};
  context_v3.config.fields.schema_version = 3;
  const caors::serialization::serialization_text_result text_v3 =
      caors::serialization::serialize_text(context_v3, actor);

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
  const caors::error result =
      caors::deserialize(context_v2, deserialized, caors::variant::object(std::move(members)));

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
  const caors::error filtered_result =
      caors::deserialize(context_v2, filtered, caors::variant::object(std::move(extra_members)));

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
      context_v3, missing_required, caors::variant::object(std::move(missing_required_members)));

  CHECK(missing_required_result.failed());
  CHECK(missing_required_result.domain() == caors::error_domain::serialization);
  CHECK(missing_required_result.value() == caors::errc::not_found);
}

// 原测试文件：serialization_reader_variant_reader_applies_required_optional_and_skipped_policies_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - variant reader applies required optional and skipped policies",
          "[serialization]") {
  REQUIRE(reflection_result_ok(((void)caors::meta::class_<reader_field_policy_actor>(
                                    "caors.test.serialization.reader_field_policy_actor"),
                                caors::reflection::class_<reader_field_policy_actor>())
                                   .field("id", &reader_field_policy_actor::id)
                                   .field("optional", &reader_field_policy_actor::optional)
                                   .field("required", &reader_field_policy_actor::required)
                                   .field("skipped", &reader_field_policy_actor::skipped)
                                   .commit()));

  caors::serialization::field_schema id = caors::serialization::make_field("id");
  caors::serialization::field_schema optional = caors::serialization::make_field("optional");
  caors::serialization::field_schema required = caors::serialization::make_field("required");
  required.required = true;
  caors::serialization::field_schema skipped = caors::serialization::make_field("skipped");
  skipped.skipped = true;

  REQUIRE(
      serialization_result_ok(caors::serialization::class_<reader_field_policy_actor>({
                                                                                          id,
                                                                                          optional,
                                                                                          required,
                                                                                          skipped,
                                                                                      })
                                  .commit()));

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

// 原测试文件：serialization_reader_variant_reader_ignores_unknown_object_members_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - variant reader ignores unknown object members", "[serialization]") {
  REQUIRE(reflection_result_ok(((void)caors::meta::class_<reader_unknown_field_actor>(
                                    "caors.test.serialization.reader_unknown_field_actor"),
                                caors::reflection::class_<reader_unknown_field_actor>())
                                   .field("id", &reader_unknown_field_actor::id)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<reader_unknown_field_actor>(
                                      {
                                          caors::serialization::make_field("id"),
                                      })
                                      .commit()));

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

// 原测试文件：serialization_reader_variant_reader_matches_fields_by_stable_field_id_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - variant reader matches fields by stable field id", "[serialization]") {
  REQUIRE(reflection_result_ok(((void)caors::meta::class_<reader_field_id_actor>(
                                    "caors.test.serialization.reader_field_id_actor"),
                                caors::reflection::class_<reader_field_id_actor>())
                                   .field("id", &reader_field_id_actor::id)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::class_<reader_field_id_actor>(
                                  {
                                      caors::serialization::make_field("id", "actor_id", false, 7),
                                  })
                                  .commit()));

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

// 原测试文件：serialization_reader_variant_reader_reports_missing_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - variant reader reports missing fields", "[serialization]") {
  REQUIRE(reflection_result_ok(((void)caors::meta::class_<reader_missing_field_actor>(
                                    "caors.test.serialization.reader_missing_field_actor"),
                                caors::reflection::class_<reader_missing_field_actor>())
                                   .field("id", &reader_missing_field_actor::id)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::class_<reader_missing_field_actor>().commit()));

  reader_missing_field_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object({}));

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::not_found);
}

// 原测试文件：serialization_reader_variant_reader_reports_missing_required_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - variant reader reports missing required fields", "[serialization]") {
  REQUIRE(reflection_result_ok(((void)caors::meta::class_<reader_field_policy_actor>(
                                    "caors.test.serialization.reader_field_policy_actor"),
                                caors::reflection::class_<reader_field_policy_actor>())
                                   .field("id", &reader_field_policy_actor::id)
                                   .field("optional", &reader_field_policy_actor::optional)
                                   .field("required", &reader_field_policy_actor::required)
                                   .field("skipped", &reader_field_policy_actor::skipped)
                                   .commit()));

  caors::serialization::field_schema id = caors::serialization::make_field("id");
  caors::serialization::field_schema optional = caors::serialization::make_field("optional");
  caors::serialization::field_schema required = caors::serialization::make_field("required");
  required.required = true;
  caors::serialization::field_schema skipped = caors::serialization::make_field("skipped");
  skipped.skipped = true;

  REQUIRE(
      serialization_result_ok(caors::serialization::class_<reader_field_policy_actor>({
                                                                                          id,
                                                                                          optional,
                                                                                          required,
                                                                                          skipped,
                                                                                      })
                                  .commit()));

  reader_field_policy_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object({}));

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::not_found);
}
