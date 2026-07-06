// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

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
