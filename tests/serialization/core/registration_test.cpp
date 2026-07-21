// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_registration_type_builder_can_attach_to_explicit_meta_type_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - registration builder commits meta and schema together",
          "[serialization]") {
  const caors::error status = caors::registration::type_<explicit_meta_serialization_actor>(
                                  "caors.test.serialization.explicit_meta_actor")
                                  .meta()
                                  .property("id", &explicit_meta_serialization_actor::id)
                                  .serialization()
                                  .field("id")
                                  .commit();
  REQUIRE(registration_result_ok(status));

  const caors::meta::type type = caors::meta::find<explicit_meta_serialization_actor>();
  REQUIRE(type.valid());
  CHECK(caors::meta::find<explicit_meta_serialization_actor>().id() == type.id());
  CHECK(caors::serialization::contains(type));

  const explicit_meta_serialization_actor actor{42};
  const caors::serialization::serialization_text_result text = caors::serialization::to_text(actor);

  REQUIRE(text.succeeded());
  CHECK(*text == R"({"id":42})");
}

// 原测试文件：serialization_registration_error_registration_can_happen_before_reflection_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - registration commits schema and fields atomically", "[serialization]") {
  REQUIRE(registration_result_ok(caors::registration::type_<serialization_first_actor>(
                                     "caors.test.serialization.serialization_first_actor")
                                     .meta()
                                     .property("id", &serialization_first_actor::id)
                                     .serialization()
                                     .field("id", "actor_id")
                                     .commit()));

  const serialization_first_actor actor{9};
  const caors::serialization::serialization_text_result result =
      caors::serialization::to_text(actor);

  REQUIRE(result.succeeded());
  CHECK(*result == R"({"actor_id":9})");
}

// 原测试文件：serialization_registration_error_rejects_weak_pointer_deserialization_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects weak pointer deserialization", "[serialization]") {
  REQUIRE(registration_result_ok(
      caors::registration::type_<weak_ptr_actor>("caors.test.serialization.weak_ptr_actor")
          .meta()
          .property("value", &weak_ptr_actor::value)
          .serialization()
          .commit()));

  weak_ptr_actor actor{};
  const caors::error result = caors::read_json_from(actor, R"({"value":7})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
  CHECK(actor.value.expired());
}

// 原测试文件：serialization_registration_error_reports_missing_configured_reflection_field_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - reports missing configured reflection field", "[serialization]") {
  const caors::error status =
      caors::registration::type_<missing_field_actor>("caors.test.serialization.missing_field")
          .meta()
          .property("id", &missing_field_actor::id)
          .serialization()
          .field("missing")
          .commit();

  CHECK(status.failed());
  CHECK(status.domain() == caors::error_domain::serialization);
  CHECK(status.value() == caors::errc::not_found);
}

// 原测试文件：serialization_registration_error_reports_missing_serialization_metadata_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - reports missing serialization metadata", "[serialization]") {
  REQUIRE(registration_result_ok(
      caors::registration::type_<reflected_only_actor>("caors.test.serialization.reflected_only")
          .meta()
          .property("id", &reflected_only_actor::id)
          .commit()));

  const reflected_only_actor actor{3};
  const caors::serialization::serialization_text_result result =
      caors::serialization::to_text(actor);

  CHECK(result.failed());
  CHECK(result.error().domain() == caors::error_domain::serialization);
  CHECK(result.error().value() == caors::errc::missing_type);
  CHECK(result->empty());
}

// 原测试文件：serialization_registration_error_reports_unsupported_reflected_field_kind_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - reports unsupported reflected field kind", "[serialization]") {
  REQUIRE(registration_result_ok(caors::registration::type_<unsupported_field_actor>(
                                     "caors.test.serialization.unsupported_field")
                                     .meta()
                                     .property("value", &unsupported_field_actor::value)
                                     .serialization()
                                     .commit()));

  const unsupported_field_actor actor{{1}};
  const caors::serialization::serialization_text_result result =
      caors::serialization::to_text(actor);

  CHECK(result.failed());
  CHECK(result.error().domain() == caors::error_domain::serialization);
  CHECK(result.error().value() == caors::errc::unsupported);
}

// 原测试文件：serialization_registration_error_reports_unsupported_weak_pointer_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - reports unsupported weak pointer fields", "[serialization]") {
  REQUIRE(registration_result_ok(
      caors::registration::type_<weak_ptr_actor>("caors.test.serialization.weak_ptr_actor")
          .meta()
          .property("value", &weak_ptr_actor::value)
          .serialization()
          .commit()));

  const std::shared_ptr<int> value = std::make_shared<int>(7);
  const weak_ptr_actor actor{value};
  const caors::serialization::serialization_text_result result =
      caors::serialization::to_text(actor);

  CHECK(result.failed());
  CHECK(result.error().domain() == caors::error_domain::serialization);
  CHECK(result.error().value() == caors::errc::unsupported);
}

// 原测试文件：serialization_registration_explicit_registration_reports_missing_meta_type_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - registration requires a valid named type", "[serialization]") {
  const caors::error status =
      caors::registration::type_<explicit_meta_serialization_actor>("").serialization().commit();

  CHECK(status.failed());
  CHECK(status.domain() == caors::error_domain::meta);
  CHECK(status.value() == caors::errc::invalid_argument);
}

// 原测试文件：serialization_registration_metadata_renames_and_skips_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - field metadata renames and skips fields", "[serialization]") {
  REQUIRE(registration_result_ok(
      caors::registration::type_<configured_actor>("caors.test.serialization.configured_actor")
          .meta()
          .property("id", &configured_actor::id)
          .property("health", &configured_actor::health)
          .property("debug", &configured_actor::debug)
          .serialization()
          .field("health", "hp")
          .field("debug", {}, true)
          .field("id")
          .commit()));

  const configured_actor actor{7, 42.5F, true};
  const caors::serialization::serialization_text_result result =
      caors::serialization::to_text(actor);

  REQUIRE(result.succeeded());
  CHECK(*result == R"({"hp":42.5,"id":7})");
}

// 原测试文件：serialization_registration_registers_type_metadata_component_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - registers type metadata component", "[serialization]") {
  const caors::error status =
      caors::registration::type_<serializable_actor>("caors.test.serialization.actor")
          .meta()
          .property("id", &serializable_actor::id)
          .property("health", &serializable_actor::health)
          .property("active", &serializable_actor::active)
          .property("name", &serializable_actor::name)
          .serialization()
          .commit();

  CHECK(registration_result_ok(status));
  const caors::meta::type type = caors::meta::find<serializable_actor>();
  CHECK(caors::serialization::contains(type));
  CHECK(caors::serialization::find_schema(type));
  CHECK(caors::serialization::size() >= 1);

  const caors::error duplicate =
      caors::registration::type_<serializable_actor>("caors.test.serialization.actor")
          .serialization()
          .commit();
  CHECK(duplicate.failed());
  CHECK(duplicate.domain() == caors::error_domain::meta);
  CHECK(duplicate.value() == caors::errc::duplicate_id);
}
