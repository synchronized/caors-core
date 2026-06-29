// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization.h"

#include <catch2/catch_all.hpp>

#include <string>
#include <string_view>

namespace {

struct serializable_actor {
  int id = 0;
  float health = 0.0F;
  bool active = false;
  std::string name{};
};

struct configured_actor {
  int id = 0;
  float health = 0.0F;
  bool debug = false;
};

struct serialization_first_actor {
  int id = 0;
};

struct reflected_only_actor {
  int id = 0;
};

struct missing_field_actor {
  int id = 0;
};

struct unsupported_field_value {
  int id = 0;
};

struct unsupported_field_actor {
  unsupported_field_value value{};
};

[[nodiscard]] bool reflection_result_ok(caors::reflection_register_result result) noexcept {
  return result == caors::reflection_register_result::success ||
         result == caors::reflection_register_result::duplicate_reflection;
}

[[nodiscard]] bool serialization_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_component;
}

} // namespace

namespace caors {

template <> struct type_name<unsupported_field_value> {
  static constexpr std::string_view value = "caors.test.serialization.unsupported_value";
};

} // namespace caors

TEST_CASE("serialization - registers type metadata component", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<serializable_actor>("caors.test.serialization.actor")
          .field("id", &serializable_actor::id)
          .field("health", &serializable_actor::health)
          .field("active", &serializable_actor::active)
          .field("name", &serializable_actor::name)
          .commit()));

  const caors::error status = caors::register_serialization<serializable_actor>();

  CHECK(serialization_result_ok(status));
  CHECK(caors::get_serialization_registry().contains(caors::find_type<serializable_actor>()->id()));
  CHECK(caors::get_serialization_registry().size() >= 1);

  const caors::error duplicate = caors::register_serialization<serializable_actor>();
  CHECK(duplicate.failed());
  CHECK(duplicate.domain() == caors::error_domain::serialization);
  CHECK(duplicate.value() == caors::errc::duplicate_component);
}

TEST_CASE("serialization - serializes reflected struct fields to text", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<serializable_actor>("caors.test.serialization.actor")
          .field("id", &serializable_actor::id)
          .field("health", &serializable_actor::health)
          .field("active", &serializable_actor::active)
          .field("name", &serializable_actor::name)
          .commit()));

  const caors::error register_result = caors::register_serialization<serializable_actor>();
  REQUIRE(serialization_result_ok(register_result));

  const serializable_actor actor{7, 95.5F, true, "Ada"};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(result.text == R"({"id":7,"health":95.5,"active":true,"name":"Ada"})");
}

TEST_CASE("serialization - field metadata renames and skips fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<configured_actor>("caors.test.serialization.configured_actor")
          .field("id", &configured_actor::id)
          .field("health", &configured_actor::health)
          .field("debug", &configured_actor::debug)
          .commit()));

  REQUIRE(caors::register_serialization<configured_actor>(
              {
                  caors::make_serialization_field("health", "hp"),
                  caors::make_serialization_field("debug", {}, true),
                  caors::make_serialization_field("id"),
              })
              .succeeded());

  const configured_actor actor{7, 42.5F, true};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(result.text == R"({"hp":42.5,"id":7})");
}

TEST_CASE("serialization - registration can happen before reflection fields", "[serialization]") {
  REQUIRE(caors::register_serialization<serialization_first_actor>(
              {
                  caors::make_serialization_field("id", "actor_id"),
              })
              .succeeded());

  REQUIRE(reflection_result_ok(caors::reflection::class_<serialization_first_actor>(
                                   "caors.test.serialization.serialization_first_actor")
                                   .field("id", &serialization_first_actor::id)
                                   .commit()));

  const serialization_first_actor actor{9};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(result.text == R"({"actor_id":9})");
}

TEST_CASE("serialization - reports missing serialization metadata", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<reflected_only_actor>("caors.test.serialization.reflected_only")
          .field("id", &reflected_only_actor::id)
          .commit()));

  const reflected_only_actor actor{3};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::missing_type);
  CHECK(result.text.empty());
}

TEST_CASE("serialization - reports missing configured reflection field", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<missing_field_actor>("caors.test.serialization.missing_field")
          .field("id", &missing_field_actor::id)
          .commit()));
  REQUIRE(caors::register_serialization<missing_field_actor>(
              {
                  caors::make_serialization_field("missing"),
              })
              .succeeded());

  const missing_field_actor actor{3};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::not_found);
}

TEST_CASE("serialization - reports unsupported reflected field kind", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<unsupported_field_actor>(
                                   "caors.test.serialization.unsupported_field")
                                   .field("value", &unsupported_field_actor::value)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<unsupported_field_actor>()));

  const unsupported_field_actor actor{{1}};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::unsupported);
}
