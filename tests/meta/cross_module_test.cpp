// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection/builder.h"
#include "caors/serialization.h"

#include <catch2/catch_all.hpp>

namespace {

struct cross_module_actor {
  int id = 0;
  float health = 0.0F;
};

struct cross_module_serialization_first {
  int id = 0;
};

struct cross_module_late_registered {
  int value = 0;
};

[[nodiscard]] bool reflection_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_reflection;
}

[[nodiscard]] bool serialization_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_component;
}

template <typename T> void register_common_type(std::string_view name) {
  const caors::error status = caors::meta::type_<T>(name).result();
  (void)status;
}

[[maybe_unused]] const bool common_cross_module_types_registered = [] {
  register_common_type<int>("int");
  register_common_type<float>("float");
  return true;
}();

template <typename View> [[nodiscard]] bool view_contains(const View& view, caors::type_id id) {
  for (const caors::type_id candidate : view.ids()) {
    if (candidate == id) {
      return true;
    }
  }

  return false;
}

} // namespace

TEST_CASE("meta - view finds types with reflection and serialization components", "[meta]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<cross_module_actor>("caors.test.meta.cross_module_actor")
          .field("id", &cross_module_actor::id)
          .field("health", &cross_module_actor::health)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<cross_module_actor>({
      caors::serialization::make_field("id"),
      caors::serialization::make_field("health"),
  })));

  const auto type = caors::meta::find<cross_module_actor>();
  REQUIRE(type);

  auto view = caors::meta::get_registry().view<caors::serialization::type_data>();
  CHECK(view_contains(view, type.id()));

  auto reflection = caors::reflection::find(type.id());
  REQUIRE(reflection);
  CHECK(reflection.field_count() == 2);

  auto serialization = view.find<caors::serialization::type_data>(type.id());
  REQUIRE(serialization.has_value());
  CHECK(serialization->fields.size() == 2);
}

TEST_CASE("meta - cross module view supports serialization registered before reflection",
          "[meta]") {
  const caors::meta::type type = caors::meta::type_<cross_module_serialization_first>(
                                          "caors.test.meta.cross_module_serialization_first")
                                          .get();
  REQUIRE(type.valid());

  REQUIRE(serialization_result_ok(
      caors::serialization::register_type<cross_module_serialization_first>({
          caors::serialization::make_field("id", "actor_id"),
      })));
  REQUIRE(reflection_result_ok(caors::reflection::class_<cross_module_serialization_first>(
                                   "caors.test.meta.cross_module_serialization_first")
                                   .field("id", &cross_module_serialization_first::id)
                                   .commit()));

  auto view = caors::meta::get_registry().view<caors::serialization::type_data>();
  CHECK(view_contains(view, type.id()));

  const cross_module_serialization_first instance{42};
  const caors::serialization::serialization_text_result result = caors::serialize_text(instance);
  REQUIRE(result.succeeded());
  CHECK(result.value == R"({"actor_id":42})");
}

TEST_CASE("reflection - field handles remain valid after later registrations", "[reflection]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<cross_module_actor>("caors.test.meta.cross_module_actor")
          .field("id", &cross_module_actor::id)
          .field("health", &cross_module_actor::health)
          .commit()));

  auto reflected = caors::reflection::find("caors.test.meta.cross_module_actor");
  REQUIRE(reflected);

  auto id = reflected.find_field("id");
  REQUIRE(id);
  REQUIRE(id.valid());

  REQUIRE(reflection_result_ok(caors::reflection::class_<cross_module_late_registered>(
                                   "caors.test.meta.cross_module_late_registered")
                                   .field("value", &cross_module_late_registered::value)
                                   .commit()));

  const cross_module_actor instance{7, 12.0F};
  const auto value = id.try_get<int>(&instance);
  REQUIRE(value.has_value());
  CHECK(*value == 7);
  CHECK(id.valid());
}
