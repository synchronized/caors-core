// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_registration_class_builder_can_attach_to_explicit_meta_type_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - class builder can attach to explicit meta type", "[serialization]") {
  const caors::meta::type type = caors::meta::class_<explicit_meta_serialization_actor>(
                                     "caors.test.serialization.explicit_meta_actor")
                                     .get();
  REQUIRE(type.valid());
  CHECK(caors::meta::find<explicit_meta_serialization_actor>().id() == type.id());

  REQUIRE(reflection_result_ok(caors::reflection::class_<explicit_meta_serialization_actor>()
                                   .field("id", &explicit_meta_serialization_actor::id)
                                   .commit()));

  const caors::error status = caors::serialization::class_(type).field("id").commit();
  REQUIRE(serialization_result_ok(status));

  const explicit_meta_serialization_actor actor{42};
  const caors::serialization::serialization_text_result text =
      caors::serialization::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(*text == R"({"id":42})");
}

// 原测试文件：serialization_registration_error_registration_can_happen_before_reflection_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - registration can happen before reflection fields", "[serialization]") {
  const caors::meta::type type = caors::meta::class_<serialization_first_actor>(
                                     "caors.test.serialization.serialization_first_actor")
                                     .get();
  REQUIRE(type.valid());

  REQUIRE(caors::serialization::class_<serialization_first_actor>(
              {
                  caors::serialization::make_field("id", "actor_id"),
              })
              .commit()
              .succeeded());

  REQUIRE(reflection_result_ok(((void)caors::meta::class_<serialization_first_actor>(
                                    "caors.test.serialization.serialization_first_actor"),
                                caors::reflection::class_<serialization_first_actor>())
                                   .field("id", &serialization_first_actor::id)
                                   .commit()));

  const serialization_first_actor actor{9};
  const caors::serialization::serialization_text_result result =
      caors::serialization::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(*result == R"({"actor_id":9})");
}

// 原测试文件：serialization_registration_error_rejects_weak_pointer_deserialization_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects weak pointer deserialization", "[serialization]") {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::class_<weak_ptr_actor>("caors.test.serialization.weak_ptr_actor"),
       caors::reflection::class_<weak_ptr_actor>())
          .field("value", &weak_ptr_actor::value)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<weak_ptr_actor>().commit()));

  weak_ptr_actor actor{};
  const caors::error result = caors::deserialize(actor, R"({"value":7})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::unsupported);
  CHECK(actor.value.expired());
}

// 原测试文件：serialization_registration_error_reports_missing_configured_reflection_field_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - reports missing configured reflection field", "[serialization]") {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::class_<missing_field_actor>("caors.test.serialization.missing_field"),
       caors::reflection::class_<missing_field_actor>())
          .field("id", &missing_field_actor::id)
          .commit()));
  REQUIRE(caors::serialization::class_<missing_field_actor>(
              {
                  caors::serialization::make_field("missing"),
              })
              .commit()
              .succeeded());

  const missing_field_actor actor{3};
  const caors::serialization::serialization_text_result result =
      caors::serialization::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.error().domain() == caors::error_domain::serialization);
  CHECK(result.error().value() == caors::errc::not_found);
}

// 原测试文件：serialization_registration_error_reports_missing_serialization_metadata_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - reports missing serialization metadata", "[serialization]") {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::class_<reflected_only_actor>("caors.test.serialization.reflected_only"),
       caors::reflection::class_<reflected_only_actor>())
          .field("id", &reflected_only_actor::id)
          .commit()));

  const reflected_only_actor actor{3};
  const caors::serialization::serialization_text_result result =
      caors::serialization::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.error().domain() == caors::error_domain::serialization);
  CHECK(result.error().value() == caors::errc::missing_type);
  CHECK(result->empty());
}

// 原测试文件：serialization_registration_error_reports_unsupported_reflected_field_kind_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - reports unsupported reflected field kind", "[serialization]") {
  REQUIRE(reflection_result_ok(((void)caors::meta::class_<unsupported_field_actor>(
                                    "caors.test.serialization.unsupported_field"),
                                caors::reflection::class_<unsupported_field_actor>())
                                   .field("value", &unsupported_field_actor::value)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::class_<unsupported_field_actor>().commit()));

  const unsupported_field_actor actor{{1}};
  const caors::serialization::serialization_text_result result =
      caors::serialization::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.error().domain() == caors::error_domain::serialization);
  CHECK(result.error().value() == caors::errc::unsupported);
}

// 原测试文件：serialization_registration_error_reports_unsupported_weak_pointer_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - reports unsupported weak pointer fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::class_<weak_ptr_actor>("caors.test.serialization.weak_ptr_actor"),
       caors::reflection::class_<weak_ptr_actor>())
          .field("value", &weak_ptr_actor::value)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<weak_ptr_actor>().commit()));

  const std::shared_ptr<int> value = std::make_shared<int>(7);
  const weak_ptr_actor actor{value};
  const caors::serialization::serialization_text_result result =
      caors::serialization::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.error().domain() == caors::error_domain::serialization);
  CHECK(result.error().value() == caors::errc::unsupported);
}

// 原测试文件：serialization_registration_explicit_registration_reports_missing_meta_type_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - explicit registration reports missing meta type", "[serialization]") {
  const caors::error status = caors::serialization::class_(caors::meta::type{}).commit();

  CHECK(status.failed());
  CHECK(status.domain() == caors::error_domain::serialization);
  CHECK(status.value() == caors::errc::invalid_type);
}

// 原测试文件：serialization_registration_field_metadata_renames_and_skips_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - field metadata renames and skips fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::class_<configured_actor>("caors.test.serialization.configured_actor"),
       caors::reflection::class_<configured_actor>())
          .field("id", &configured_actor::id)
          .field("health", &configured_actor::health)
          .field("debug", &configured_actor::debug)
          .commit()));

  REQUIRE(caors::serialization::class_<configured_actor>(
              {
                  caors::serialization::make_field("health", "hp"),
                  caors::serialization::make_field("debug", {}, true),
                  caors::serialization::make_field("id"),
              })
              .commit()
              .succeeded());

  const configured_actor actor{7, 42.5F, true};
  const caors::serialization::serialization_text_result result =
      caors::serialization::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(*result == R"({"hp":42.5,"id":7})");
}

// 原测试文件：serialization_registration_registers_type_metadata_component_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - registers type metadata component", "[serialization]") {
  REQUIRE(reflection_result_ok(
      ((void)caors::meta::class_<serializable_actor>("caors.test.serialization.actor"),
       caors::reflection::class_<serializable_actor>())
          .field("id", &serializable_actor::id)
          .field("health", &serializable_actor::health)
          .field("active", &serializable_actor::active)
          .field("name", &serializable_actor::name)
          .commit()));

  const caors::error status = caors::serialization::class_<serializable_actor>().commit();

  CHECK(serialization_result_ok(status));
  CHECK(caors::serialization::contains(caors::meta::find<serializable_actor>().id()));
  CHECK(caors::serialization::size() >= 1);

  const caors::error duplicate = caors::serialization::class_<serializable_actor>().commit();
  CHECK(duplicate.failed());
  CHECK(duplicate.domain() == caors::error_domain::serialization);
  CHECK(duplicate.value() == caors::errc::duplicate_component);
}
