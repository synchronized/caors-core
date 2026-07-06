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
