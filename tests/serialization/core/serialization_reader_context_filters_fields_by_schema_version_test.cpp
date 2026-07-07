// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

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
  caors::serialization::context context_v2{};
  context_v2.fields.schema_version = 2;
  const caors::serialization::serialization_text_result text_v2 =
      caors::serialize_text(actor, context_v2);

  REQUIRE(text_v2.succeeded());
  CHECK(text_v2.value == R"({"id":1,"current":20})");

  caors::serialization::context context_v3{};
  context_v3.fields.schema_version = 3;
  const caors::serialization::serialization_text_result text_v3 =
      caors::serialize_text(actor, context_v3);

  REQUIRE(text_v3.succeeded());
  CHECK(text_v3.value == R"({"id":1,"current":20,"future":30})");

  caors::serialization::context mixed_context{};
  mixed_context.schema_version = 2;
  mixed_context.fields.schema_version = 3;
  const caors::serialization::serialization_text_result mixed_text =
      caors::serialize_text(actor, mixed_context);

  REQUIRE(mixed_text.succeeded());
  CHECK(mixed_text.value == R"({"id":1,"current":20,"future":30})");

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
      caors::deserialize(deserialized, caors::variant::object(std::move(members)), context_v2);

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
      caors::deserialize(filtered, caors::variant::object(std::move(extra_members)), context_v2);

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
      missing_required, caors::variant::object(std::move(missing_required_members)), context_v3);

  CHECK(missing_required_result.failed());
  CHECK(missing_required_result.domain() == caors::error_domain::serialization);
  CHECK(missing_required_result.value() == caors::errc::not_found);
}
