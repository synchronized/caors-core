// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_read_parses_json_and_deserializes_reflected_struct_fields_test.cpp

#include "serialization_test_support.h"

#include "caors/io/memory_file_system.h"
#include "caors/io/stream.h"
#include "caors/memory/tracking.h"
#include "caors/serialization/config_validation.h"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <utility>

using namespace caors::tests::serialization;

namespace {

struct schema_config_actor {
  caors::string format{};
  int priority = 0;
};

struct schema_policy_actor {
  int id = 0;
  int score = 0;
};

struct schema_policy_child {
  int id = 0;
  int score = 0;
};

struct schema_policy_parent {
  int id = 0;
  schema_policy_child child{};
  caors::vector<schema_policy_child> children{};
};

struct schema_default_actor {
  caors::string format{};
  int priority = 0;
};

struct schema_partial_actor {
  caors::string format{};
  int priority = 5;
  bool enabled = true;
};

void require_schema_config_actor_metadata() {
  REQUIRE(registration_result_ok(
      caors::registration::type_<schema_config_actor>(
          "caors.test.serialization.schema_config_actor")
          .meta()
          .property("format", &schema_config_actor::format, caors::meta::registration::non_empty(),
                    caors::meta::registration::one_of({"json", "binary"}),
                    caors::meta::registration::default_value("json"))
          .property("priority", &schema_config_actor::priority,
                    caors::meta::registration::range(1, 10),
                    caors::meta::registration::default_value(5))
          .commit()));
}

void require_schema_policy_actor_metadata() {
  caors::serialization::field_options id = caors::serialization::make_field("id");
  id.field_id = 1;
  caors::serialization::field_options score = caors::serialization::make_field("score");
  score.field_id = 7;
  score.legacy_serialized_name = caors::string_name{"points"};
  score.legacy_field_id = 3;

  REQUIRE(registration_result_ok(caors::registration::type_<schema_policy_actor>(
                                     "caors.test.serialization.schema_policy_actor")
                                     .meta()
                                     .property("id", &schema_policy_actor::id)
                                     .property("score", &schema_policy_actor::score)
                                     .serialization()
                                     .fields({id, score})
                                     .commit()));
}

void require_schema_policy_parent_metadata() {
  caors::serialization::field_options child_id = caors::serialization::make_field("id");
  child_id.field_id = 1;
  caors::serialization::field_options child_score = caors::serialization::make_field("score");
  child_score.field_id = 7;
  child_score.legacy_serialized_name = caors::string_name{"points"};
  child_score.legacy_field_id = 3;

  REQUIRE(registration_result_ok(caors::registration::type_<schema_policy_child>(
                                     "caors.test.serialization.schema_policy_child")
                                     .meta()
                                     .property("id", &schema_policy_child::id)
                                     .property("score", &schema_policy_child::score)
                                     .serialization()
                                     .fields({child_id, child_score})
                                     .commit()));

  REQUIRE(registration_result_ok(caors::registration::type_<schema_policy_parent>(
                                     "caors.test.serialization.schema_policy_parent")
                                     .meta()
                                     .property("id", &schema_policy_parent::id)
                                     .property("child", &schema_policy_parent::child)
                                     .property("children", &schema_policy_parent::children)
                                     .serialization()
                                     .commit()));
}

void require_schema_default_actor_metadata() {
  caors::serialization::field_options format = caors::serialization::make_field("format");
  caors::serialization::field_options priority = caors::serialization::make_field("priority");

  REQUIRE(registration_result_ok(caors::registration::type_<schema_default_actor>(
                                     "caors.test.serialization.schema_default_actor")
                                     .meta()
                                     .property("format", &schema_default_actor::format,
                                               caors::meta::registration::default_value("json"))
                                     .property("priority", &schema_default_actor::priority,
                                               caors::meta::registration::default_value(5))
                                     .serialization()
                                     .fields({format, priority})
                                     .commit()));
}

void require_schema_partial_actor_metadata() {
  REQUIRE(registration_result_ok(caors::registration::type_<schema_partial_actor>(
                                     "caors.test.serialization.schema_partial_actor")
                                     .meta()
                                     .property("format", &schema_partial_actor::format)
                                     .property("priority", &schema_partial_actor::priority)
                                     .property("enabled", &schema_partial_actor::enabled)
                                     .serialization()
                                     .commit()));
}

[[nodiscard]] caors::error
migrate_schema_policy_actor(const caors::serialization::schema_migration_request& request,
                            caors::variant& value, void*) {
  if (request.source_version != 1 || request.target_version != 2) {
    return {caors::error_domain::serialization, caors::errc::unsupported};
  }

  caors::variant::object_type* members = value.try_mutable_object();
  if (members == nullptr) {
    return {};
  }

  const caors::variant* legacy_score =
      value.find_member_variant(caors::string_name{"legacy_score"});
  if (legacy_score != nullptr &&
      value.find_member_variant(caors::string_name{"score"}) == nullptr) {
    members->push_back(caors::meta::variant_member{
        caors::string_name{"score"},
        7,
        *legacy_score,
    });
  }

  members->erase(std::remove_if(members->begin(), members->end(),
                                [](const caors::meta::variant_member& member) {
                                  return member.name == caors::string_name{"legacy_score"} ||
                                         member.name == caors::string_name{"removed"};
                                }),
                 members->end());
  return {};
}

} // namespace

TEST_CASE("serialization - parses json and deserializes reflected struct fields",
          "[serialization]") {
  require_reader_actor_metadata();

  const caors::serialization::variant_result parsed = caors::serialization::parse_json(
      R"({"id":7,"health":95.5,"active":true,"name":"Ada\nLovelace"})");
  REQUIRE(parsed.succeeded());
  REQUIRE(parsed->is_object());
  const caors::meta::variant_member* name = parsed->find_member(caors::string_name{"name"});
  REQUIRE(name != nullptr);
  REQUIRE(name->value.is_string());
  CHECK(name->value.as_string() == "Ada\nLovelace");

  reader_actor actor{};
  const caors::error result =
      caors::read_json_from(actor, R"({"id":7,"health":95.5,"active":true,"name":"Ada"})");

  REQUIRE(result.succeeded());
  CHECK(actor.id == 7);
  CHECK(actor.health == Catch::Approx(95.5));
  CHECK(actor.active);
  CHECK(actor.name == "Ada");
}

TEST_CASE("serialization - json reader deserializes unordered fields directly", "[serialization]") {
  require_reader_actor_metadata();

  caors::serialization::json_reader reader{R"({"name":"Ada","active":true,"health":95.5,"id":7})"};

  reader_actor actor{};
  const caors::error result = caors::deserialize(actor, reader);

  REQUIRE(result.succeeded());
  CHECK(actor.id == 7);
  CHECK(actor.health == Catch::Approx(95.5));
  CHECK(actor.active);
  CHECK(actor.name == "Ada");
}

TEST_CASE("serialization - json text reader applies duplicate key policy", "[serialization]") {
  require_reader_actor_metadata();

  caors::serialization::json_options options{};
  options.duplicate_key_policy = caors::serialization::json_duplicate_key_policy::use_last;

  reader_actor actor{};
  caors::error result = caors::serialization::read_json_from(
      actor, R"({"id":1,"id":2,"health":3.5,"active":true,"name":"Ada"})", options);

  REQUIRE(result.succeeded());
  CHECK(actor.id == 2);

  options.duplicate_key_policy = caors::serialization::json_duplicate_key_policy::reject;
  reader_actor rejected{};
  result = caors::serialization::read_json_from(
      rejected, R"({"id":1,"id":2,"health":3.5,"active":true,"name":"Ada"})", options);

  REQUIRE(result.failed());
  CHECK(result.value() == caors::errc::duplicate_name);
}

TEST_CASE("serialization - reads json from io file system and deserializes reflected fields",
          "[serialization]") {
  require_reader_actor_metadata();

  caors::io::memory_file_system fs;
  REQUIRE(fs.write_all("actors/ada.json", R"({"id":8,"health":88.5,"active":false,"name":"Ada"})")
              .succeeded());

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::variant_result parsed =
      caors::serialization::parse_json_from(fs, "actors/ada.json", &tracking);

  REQUIRE(parsed.succeeded());
  REQUIRE(parsed->is_object());

  reader_actor actor{};
  const caors::error result =
      caors::serialization::read_json_from(actor, fs, "actors/ada.json", &tracking);

  REQUIRE(result.succeeded());
  CHECK(actor.id == 8);
  CHECK(actor.health == Catch::Approx(88.5));
  CHECK_FALSE(actor.active);
  CHECK(actor.name == "Ada");
}

TEST_CASE("serialization - json stream convenience rewinds context scratch", "[serialization]") {
  require_reader_actor_metadata();

  std::string_view text = R"({"id":9,"health":77.5,"active":true,"name":"Grace"})";
  caors::io::byte_buffer bytes = caors::io::make_byte_buffer();
  for (const unsigned char character : text) {
    bytes.push_back(static_cast<std::byte>(character));
  }
  caors::io::input_stream_ptr stream = caors::io::make_input_stream(std::move(bytes));

  caors::tracking_resource scratch_tracking{&caors::get_system_memory_resource()};
  caors::stack_resource scratch{64, &scratch_tracking};
  caors::serialization::context context{};
  context.resolvers.memory.scratch_resource = &scratch;

  reader_actor actor{};
  REQUIRE(caors::serialization::read_json_from(context, actor, *stream).succeeded());
  CHECK(scratch.active_scope_count() == 0);
  scratch.reset();
  CHECK(scratch_tracking.statistics().bytes_in_use == 0);
  CHECK(actor.id == 9);
  CHECK(actor.name == "Grace");
}

// 原测试文件：serialization_read_rejects_invalid_char_and_enum_values_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects invalid char and enum values", "[serialization]") {
  require_scalar_actor_metadata();

  scalar_actor actor{};

  const caors::error invalid_char =
      caors::read_json_from(actor, R"({"code":"AB","color":1,"state":-1})");
  REQUIRE(invalid_char.failed());
  CHECK(invalid_char.domain() == caors::error_domain::serialization);

  const caors::error invalid_enum =
      caors::read_json_from(actor, R"({"code":"A","color":300,"state":-1})");
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
  CHECK(parsed.error().domain() == caors::error_domain::serialization);
  CHECK(parsed.error().value() == caors::errc::invalid_argument);
}

TEST_CASE("serialization - json parser applies parser side options", "[serialization]") {
  caors::serialization::json_options options{};

  options.allow_trailing_commas = true;
  caors::serialization::variant_result trailing =
      caors::serialization::parse_json(R"({"id":1,})", options);
  REQUIRE(trailing.succeeded());
  REQUIRE(trailing->is_object());
  CHECK(trailing->as_object().size() == 1U);

  options = {};
  options.max_parse_depth = 1;
  caors::serialization::variant_result shallow =
      caors::serialization::parse_json(R"([1])", options);
  REQUIRE(shallow.succeeded());
  caors::serialization::variant_result too_deep =
      caors::serialization::parse_json(R"([[1]])", options);
  REQUIRE(too_deep.failed());
  CHECK(too_deep.error().value() == caors::errc::recursion_limit);

  options = {};
  options.duplicate_key_policy = caors::serialization::json_duplicate_key_policy::reject;
  caors::serialization::variant_result duplicate =
      caors::serialization::parse_json(R"({"id":1,"id":2})", options);
  REQUIRE(duplicate.failed());
  CHECK(duplicate.error().value() == caors::errc::duplicate_name);

  options.duplicate_key_policy = caors::serialization::json_duplicate_key_policy::use_last;
  duplicate = caors::serialization::parse_json(R"({"id":1,"id":2})", options);
  REQUIRE(duplicate.succeeded());
  const caors::variant* id = duplicate->find_member_variant(caors::string_name{"id"});
  REQUIRE(id != nullptr);
  REQUIRE(id->try_as_u64() != nullptr);
  CHECK(*id->try_as_u64() == 2U);
}

TEST_CASE("serialization - json parser reads from input stream without staging text",
          "[serialization]") {
  std::string_view text = R"({"id":1,})";
  caors::io::byte_buffer bytes = caors::io::make_byte_buffer();
  for (const unsigned char character : text) {
    bytes.push_back(static_cast<std::byte>(character));
  }

  caors::io::input_stream_ptr stream = caors::io::make_input_stream(std::move(bytes));
  caors::serialization::json_options options{};
  options.allow_trailing_commas = true;

  caors::serialization::variant_result parsed = caors::serialization::parse_json(*stream, options);
  REQUIRE(parsed.succeeded());

  const caors::variant* id = parsed->find_member_variant(caors::string_name{"id"});
  REQUIRE(id != nullptr);
  REQUIRE(id->try_as_u64() != nullptr);
  CHECK(*id->try_as_u64() == 1U);
}

TEST_CASE("serialization - json stream parser uses the result resource", "[serialization]") {
  std::string_view text = R"({"items":["first","second"]})";
  caors::io::byte_buffer bytes = caors::io::make_byte_buffer();
  for (const unsigned char character : text) {
    bytes.push_back(static_cast<std::byte>(character));
  }

  caors::io::input_stream_ptr stream = caors::io::make_input_stream(std::move(bytes));
  caors::tracking_resource result_resource{&caors::get_system_memory_resource()};
  caors::serialization::variant_result parsed =
      caors::serialization::parse_json(*stream, &result_resource);
  REQUIRE(parsed.succeeded());
  CHECK(parsed->resource() == &result_resource);

  const caors::variant* items = parsed->find_member_variant(caors::string_name{"items"});
  REQUIRE(items != nullptr);
  REQUIRE(items->try_as_array() != nullptr);
  CHECK(items->resource() == &result_resource);
  REQUIRE(items->as_array().size() == 2U);
  CHECK(items->as_array()[0].resource() == &result_resource);
  CHECK(items->as_array()[1].resource() == &result_resource);
}

TEST_CASE("serialization - json parser decodes unicode escapes as utf8", "[serialization]") {
  const caors::serialization::variant_result parsed =
      caors::serialization::parse_json(R"({"text":"caf\u00e9 \ud83d\ude00"})");
  REQUIRE(parsed.succeeded());
  const caors::variant* text = parsed->find_member_variant(caors::string_name{"text"});
  REQUIRE(text != nullptr);
  REQUIRE(text->try_as_string() != nullptr);
  CHECK(*text->try_as_string() == "caf"
                                  "\xC3"
                                  "\xA9"
                                  " "
                                  "\xF0"
                                  "\x9F"
                                  "\x98"
                                  "\x80");

  const caors::serialization::variant_result invalid =
      caors::serialization::parse_json(R"({"text":"\ud83d"})");
  REQUIRE(invalid.failed());
  CHECK(invalid.error().value() == caors::errc::invalid_argument);
}

TEST_CASE("serialization - config validation uses builtin property schema", "[serialization]") {
  require_schema_config_actor_metadata();

  schema_config_actor config{};
  REQUIRE(caors::serialization::apply_config_defaults(config).succeeded());
  CHECK(config.format == "json");
  CHECK(config.priority == 5);

  caors::serialization::config_validation_result valid =
      caors::serialization::validate_config(config);
  REQUIRE(valid.succeeded());
  CHECK(valid.diagnostics.empty());

  config.format = "xml";
  config.priority = 42;
  caors::serialization::config_validation_result invalid =
      caors::serialization::validate_config(config);
  REQUIRE(invalid.failed());
  CHECK(invalid.diagnostics.size() == 2U);
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

  const caors::serialization::serialization_text_result text = caors::to_json(actor);
  REQUIRE(text.succeeded());
  CHECK(*text == R"({"code":"A","color":2,"state":2})");

  const caors::serialization::variant_result value = caors::to_value(actor);
  REQUIRE(value.succeeded());

  const caors::meta::variant_member* code = value->find_member(caors::string_name{"code"});
  const caors::meta::variant_member* color = value->find_member(caors::string_name{"color"});
  const caors::meta::variant_member* state = value->find_member(caors::string_name{"state"});
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
      caors::read_json_from(decoded, R"({"code":"Z","color":1,"state":-1})");
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
  require_reader_actor_metadata();

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{7}),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"health"},
      0,
      caors::variant::from(95.5),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"active"},
      0,
      caors::variant::from(true),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"name"},
      0,
      caors::variant::from(std::string_view{"Ada"}),
  });

  reader_actor actor{};
  const caors::error result =
      caors::read_value_from(actor, caors::variant::object(std::move(members)));

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
  caors::serialization::field_options id = caors::serialization::make_field("id");
  caors::serialization::field_options legacy = caors::serialization::make_field("legacy");
  legacy.version_added = 1;
  legacy.version_removed = 2;
  caors::serialization::field_options current = caors::serialization::make_field("current");
  current.version_added = 2;
  caors::serialization::field_options future = caors::serialization::make_field("future");
  future.version_added = 3;
  future.required = true;

  REQUIRE(registration_result_ok(
      caors::registration::type_<versioned_actor>("caors.test.serialization.versioned_actor")
          .meta()
          .property("id", &versioned_actor::id)
          .property("legacy", &versioned_actor::legacy)
          .property("current", &versioned_actor::current)
          .property("future", &versioned_actor::future)
          .serialization()
          .fields({id, legacy, current, future})
          .commit()));

  const versioned_actor actor{1, 10, 20, 30};
  caors::serialization::context context_v2{};
  context_v2.config.fields.schema_version = 2;
  const caors::serialization::serialization_text_result text_v2 =
      caors::serialization::to_text(context_v2, actor);

  REQUIRE(text_v2.succeeded());
  CHECK(*text_v2 == R"({"id":1,"current":20})");

  caors::serialization::context context_v3{};
  context_v3.config.fields.schema_version = 3;
  const caors::serialization::serialization_text_result text_v3 =
      caors::serialization::to_text(context_v3, actor);

  REQUIRE(text_v3.succeeded());
  CHECK(*text_v3 == R"({"id":1,"current":20,"future":30})");

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{2}),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"current"},
      0,
      caors::variant::from(std::int64_t{22}),
  });

  versioned_actor deserialized{};
  const caors::error result =
      caors::read_value_from(context_v2, deserialized, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(deserialized.id == 2);
  CHECK(deserialized.legacy == 0);
  CHECK(deserialized.current == 22);
  CHECK(deserialized.future == 0);

  caors::variant::object_type extra_members;
  extra_members.push_back(caors::meta::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{3}),
  });
  extra_members.push_back(caors::meta::variant_member{
      caors::string_name{"legacy"},
      0,
      caors::variant::from(std::int64_t{33}),
  });
  extra_members.push_back(caors::meta::variant_member{
      caors::string_name{"current"},
      0,
      caors::variant::from(std::int64_t{44}),
  });
  extra_members.push_back(caors::meta::variant_member{
      caors::string_name{"future"},
      0,
      caors::variant::from(std::int64_t{55}),
  });

  versioned_actor filtered{0, 10, 0, 30};
  const caors::error filtered_result = caors::read_value_from(
      context_v2, filtered, caors::variant::object(std::move(extra_members)));

  REQUIRE(filtered_result.succeeded());
  CHECK(filtered.id == 3);
  CHECK(filtered.legacy == 10);
  CHECK(filtered.current == 44);
  CHECK(filtered.future == 30);

  caors::variant::object_type missing_required_members;
  missing_required_members.push_back(caors::meta::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{4}),
  });
  missing_required_members.push_back(caors::meta::variant_member{
      caors::string_name{"current"},
      0,
      caors::variant::from(std::int64_t{55}),
  });

  versioned_actor missing_required{};
  const caors::error missing_required_result = caors::read_value_from(
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
  caors::serialization::field_options id = caors::serialization::make_field("id");
  caors::serialization::field_options optional = caors::serialization::make_field("optional");
  caors::serialization::field_options required = caors::serialization::make_field("required");
  required.required = true;
  caors::serialization::field_options skipped = caors::serialization::make_field("skipped");
  skipped.skipped = true;

  REQUIRE(registration_result_ok(caors::registration::type_<reader_field_policy_actor>(
                                     "caors.test.serialization.reader_field_policy_actor")
                                     .meta()
                                     .property("id", &reader_field_policy_actor::id)
                                     .property("optional", &reader_field_policy_actor::optional)
                                     .property("required", &reader_field_policy_actor::required)
                                     .property("skipped", &reader_field_policy_actor::skipped)
                                     .serialization()
                                     .fields({id, optional, required, skipped})
                                     .commit()));

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{1}),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"required"},
      0,
      caors::variant::from(std::int64_t{3}),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"skipped"},
      0,
      caors::variant::from(std::int64_t{4}),
  });

  reader_field_policy_actor actor{};
  const caors::error result =
      caors::read_value_from(actor, caors::variant::object(std::move(members)));

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
  REQUIRE(registration_result_ok(caors::registration::type_<reader_unknown_field_actor>(
                                     "caors.test.serialization.reader_unknown_field_actor")
                                     .meta()
                                     .property("id", &reader_unknown_field_actor::id)
                                     .serialization()
                                     .field("id")
                                     .commit()));

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{7}),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"extra"},
      0,
      caors::variant::from(std::int64_t{99}),
  });

  reader_unknown_field_actor actor{};
  const caors::error result =
      caors::read_value_from(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id == 7);
}

// 原测试文件：serialization_reader_variant_reader_matches_fields_by_stable_field_id_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - variant reader matches fields by stable field id", "[serialization]") {
  REQUIRE(registration_result_ok(caors::registration::type_<reader_field_id_actor>(
                                     "caors.test.serialization.reader_field_id_actor")
                                     .meta()
                                     .property("id", &reader_field_id_actor::id)
                                     .serialization()
                                     .field("id", "actor_id", false, 7)
                                     .commit()));

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"legacy_id"},
      7,
      caors::variant::from(std::int64_t{42}),
  });

  reader_field_id_actor actor{};
  const caors::error result =
      caors::read_value_from(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id == 42);
}

TEST_CASE("serialization - schema policies support legacy fields and unknown members",
          "[serialization]") {
  require_schema_policy_actor_metadata();

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"id"},
      1,
      caors::variant::from(std::int64_t{7}),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"points"},
      3,
      caors::variant::from(std::int64_t{99}),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"extra"},
      0,
      caors::variant::from(std::int64_t{123}),
  });
  const caors::variant value = caors::variant::object(std::move(members));

  schema_policy_actor ignored{};
  REQUIRE(caors::read_value_from(ignored, value).succeeded());
  CHECK(ignored.id == 7);
  CHECK(ignored.score == 99);

  schema_policy_actor rejected{};
  caors::serialization::context reject_context{};
  reject_context.config.schema.unknown_fields = caors::serialization::unknown_field_policy::reject;
  const caors::error rejected_status = caors::read_value_from(reject_context, rejected, value);
  REQUIRE(rejected_status.failed());
  CHECK(rejected_status.domain() == caors::error_domain::serialization);
  CHECK(rejected_status.value() == caors::errc::invalid_field);

  schema_policy_actor preserved{};
  caors::variant::object_type preserved_unknown_fields;
  caors::serialization::context preserve_context{};
  preserve_context.config.schema.unknown_fields =
      caors::serialization::unknown_field_policy::preserve;
  preserve_context.state.schema.preserved_unknown_fields = &preserved_unknown_fields;
  REQUIRE(caors::read_value_from(preserve_context, preserved, value).succeeded());
  CHECK(preserved.id == 7);
  CHECK(preserved.score == 99);
  REQUIRE(preserved_unknown_fields.size() == 1U);
  CHECK(preserved_unknown_fields.front().name == caors::string_name{"extra"});
}

TEST_CASE("serialization - schema unknown field preservation records nested paths",
          "[serialization]") {
  require_schema_policy_parent_metadata();

  caors::tracking_resource result_tracking{&caors::get_system_memory_resource()};
  caors::tracking_resource temporary_tracking{&caors::get_system_memory_resource()};
  caors::stack_resource temporary{64, &temporary_tracking};

  caors::variant::object_type child_members;
  child_members.push_back(caors::meta::variant_member{
      caors::string_name{"id"},
      1,
      caors::variant::from(std::int64_t{2}),
  });
  child_members.push_back(caors::meta::variant_member{
      caors::string_name{"extra_child"},
      0,
      caors::variant::from(std::int64_t{20}),
  });

  caors::variant::object_type array_child_members;
  array_child_members.push_back(caors::meta::variant_member{
      caors::string_name{"id"},
      1,
      caors::variant::from(std::int64_t{3}),
  });
  array_child_members.push_back(caors::meta::variant_member{
      caors::string_name{"extra_array"},
      0,
      caors::variant::from(std::int64_t{30}),
  });

  caors::variant::array_type children;
  children.push_back(caors::variant::object(std::move(array_child_members)));

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{1}),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"child"},
      0,
      caors::variant::object(std::move(child_members)),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"children"},
      0,
      caors::variant::array(std::move(children)),
  });

  schema_policy_parent actor{};
  caors::vector<caors::serialization::preserved_unknown_field> preserved_entries;
  caors::serialization::context context{};
  context.config.schema.unknown_fields = caors::serialization::unknown_field_policy::preserve;
  context.resolvers.memory.result_resource = &result_tracking;
  context.resolvers.memory.scratch_resource = &temporary;
  context.state.schema.preserved_unknown_field_entries = &preserved_entries;

  REQUIRE(caors::read_value_from(context, actor, caors::variant::object(std::move(members)))
              .succeeded());

  CHECK(actor.id == 1);
  CHECK(actor.child.id == 2);
  REQUIRE(actor.children.size() == 1U);
  CHECK(actor.children.front().id == 3);
  REQUIRE(preserved_entries.size() == 2U);

  CHECK(preserved_entries[0].path.size() == 1U);
  CHECK(preserved_entries[0].path[0].kind ==
        caors::serialization::schema_path_segment_kind::member);
  CHECK(preserved_entries[0].path[0].name == caors::string_name{"child"});
  CHECK(preserved_entries[0].member.name == caors::string_name{"extra_child"});

  CHECK(preserved_entries[1].path.size() == 2U);
  CHECK(preserved_entries[1].path[0].name == caors::string_name{"children"});
  CHECK(preserved_entries[1].path[1].kind ==
        caors::serialization::schema_path_segment_kind::element);
  CHECK(preserved_entries[1].path[1].index == 0U);
  CHECK(preserved_entries[1].member.name == caors::string_name{"extra_array"});

  CHECK(temporary.active_scope_count() == 0);
  CHECK(temporary.statistics().mark_count > 0);
  CHECK(temporary.statistics().mark_count == temporary.statistics().rewind_count);
  temporary.reset();
  CHECK(temporary_tracking.statistics().bytes_in_use == 0);

  // 保留路径必须使用结果资源，临时资源回滚后仍然有效。
  CHECK(preserved_entries[0].path[0].name == caors::string_name{"child"});
  CHECK(preserved_entries[1].path[0].name == caors::string_name{"children"});
}

TEST_CASE("serialization - schema unknown field rejection checks nested objects",
          "[serialization]") {
  require_schema_policy_parent_metadata();

  caors::variant::object_type child_members;
  child_members.push_back(caors::meta::variant_member{
      caors::string_name{"extra_child"},
      0,
      caors::variant::from(std::int64_t{20}),
  });

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{1}),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"child"},
      0,
      caors::variant::object(std::move(child_members)),
  });

  schema_policy_parent actor{};
  caors::serialization::context context{};
  context.config.schema.unknown_fields = caors::serialization::unknown_field_policy::reject;

  const caors::error result =
      caors::read_value_from(context, actor, caors::variant::object(std::move(members)));

  REQUIRE(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_field);
}

TEST_CASE("serialization - json reader accepts legacy field names", "[serialization]") {
  require_schema_policy_actor_metadata();

  schema_policy_actor actor{};
  const caors::error result = caors::read_json_from(actor, R"({"id":8,"points":64})");

  REQUIRE(result.succeeded());
  CHECK(actor.id == 8);
  CHECK(actor.score == 64);
}

TEST_CASE("serialization - explicit schema applies property defaults for missing fields",
          "[serialization]") {
  require_schema_default_actor_metadata();

  schema_default_actor actor{};
  const caors::error result = caors::read_value_from(actor, caors::variant::object({}));

  REQUIRE(result.succeeded());
  CHECK(actor.format == "json");
  CHECK(actor.priority == 5);
}

TEST_CASE("serialization - schema migration can rewrite DOM before strict unknown checks",
          "[serialization]") {
  require_schema_policy_actor_metadata();

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"id"},
      1,
      caors::variant::from(std::int64_t{9}),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"legacy_score"},
      0,
      caors::variant::from(std::int64_t{77}),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"removed"},
      0,
      caors::variant::from(std::int64_t{123}),
  });

  caors::serialization::schema_migration_ops migration{&migrate_schema_policy_actor};
  caors::serialization::context context{};
  context.config.fields.schema_version = 2;
  context.config.schema.source_version = 1;
  context.config.schema.migration = &migration;
  context.config.schema.unknown_fields = caors::serialization::unknown_field_policy::reject;

  schema_policy_actor actor{};
  const caors::error result =
      caors::read_value_from(context, actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id == 9);
  CHECK(actor.score == 77);
}

TEST_CASE("serialization - schema migration is dispatched for nested object values",
          "[serialization]") {
  require_schema_policy_parent_metadata();

  caors::variant::object_type child_members;
  child_members.push_back(caors::meta::variant_member{
      caors::string_name{"id"},
      1,
      caors::variant::from(std::int64_t{2}),
  });
  child_members.push_back(caors::meta::variant_member{
      caors::string_name{"legacy_score"},
      0,
      caors::variant::from(std::int64_t{88}),
  });
  child_members.push_back(caors::meta::variant_member{
      caors::string_name{"removed"},
      0,
      caors::variant::from(std::int64_t{123}),
  });

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{1}),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"child"},
      0,
      caors::variant::object(std::move(child_members)),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"children"},
      0,
      caors::variant::array({}),
  });

  caors::serialization::schema_migration_ops migration{&migrate_schema_policy_actor};
  caors::serialization::context context{};
  context.config.fields.schema_version = 2;
  context.config.schema.source_version = 1;
  context.config.schema.migration = &migration;
  context.config.schema.unknown_fields = caors::serialization::unknown_field_policy::reject;

  schema_policy_parent actor{};
  const caors::error result =
      caors::read_value_from(context, actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id == 1);
  CHECK(actor.child.id == 2);
  CHECK(actor.child.score == 88);
}

TEST_CASE("serialization - json text can keep existing values for missing auto schema fields",
          "[serialization]") {
  require_schema_partial_actor_metadata();

  caors::serialization::context context{};
  context.config.schema.missing_fields = caors::serialization::missing_field_policy::keep_existing;

  schema_partial_actor actor{};
  actor.priority = 9;
  actor.enabled = false;

  const caors::error result = caors::read_json_from(context, actor, R"({"format":"json"})");

  REQUIRE(result.succeeded());
  CHECK(actor.format == "json");
  CHECK(actor.priority == 9);
  CHECK_FALSE(actor.enabled);
}

// 原测试文件：serialization_reader_variant_reader_reports_missing_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - variant reader reports missing fields", "[serialization]") {
  REQUIRE(registration_result_ok(caors::registration::type_<reader_missing_field_actor>(
                                     "caors.test.serialization.reader_missing_field_actor")
                                     .meta()
                                     .property("id", &reader_missing_field_actor::id)
                                     .serialization()
                                     .commit()));

  reader_missing_field_actor actor{};
  const caors::error result = caors::read_value_from(actor, caors::variant::object({}));

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::not_found);
}

// 原测试文件：serialization_reader_variant_reader_reports_missing_required_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - variant reader reports missing required fields", "[serialization]") {
  caors::serialization::field_options id = caors::serialization::make_field("id");
  caors::serialization::field_options optional = caors::serialization::make_field("optional");
  caors::serialization::field_options required = caors::serialization::make_field("required");
  required.required = true;
  caors::serialization::field_options skipped = caors::serialization::make_field("skipped");
  skipped.skipped = true;

  REQUIRE(registration_result_ok(caors::registration::type_<reader_field_policy_actor>(
                                     "caors.test.serialization.reader_field_policy_actor")
                                     .meta()
                                     .property("id", &reader_field_policy_actor::id)
                                     .property("optional", &reader_field_policy_actor::optional)
                                     .property("required", &reader_field_policy_actor::required)
                                     .property("skipped", &reader_field_policy_actor::skipped)
                                     .serialization()
                                     .fields({id, optional, required, skipped})
                                     .commit()));

  reader_field_policy_actor actor{};
  const caors::error result = caors::read_value_from(actor, caors::variant::object({}));

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::not_found);
}
