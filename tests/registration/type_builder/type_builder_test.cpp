// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/registration.h"

#include "caors/meta/method.h"
#include "caors/meta/property.h"

#include <catch2/catch_all.hpp>

#include <optional>

namespace {

struct registration_actor final : caors::object {
  int value = 0;

  void set_value(int next) { value = next; }
};

} // namespace

TEST_CASE("registration - type builder registers meta serialization and object together",
          "[registration]") {
  const caors::error status =
      caors::registration::type_<registration_actor>("caors.test.registration.actor")
          .meta()
          .property("value", &registration_actor::value)
          .method("set_value", &registration_actor::set_value)
          .serialization()
          .field("value")
          .object()
          .commit();

  REQUIRE(status == caors::error{});

  const caors::meta::type meta_type = caors::meta::find<registration_actor>();
  REQUIRE(meta_type.registered());
  CHECK(meta_type.name() == caors::core::string_name{"caors.test.registration.actor"});
  CHECK(static_cast<bool>(meta_type.find_property("value")));
  CHECK(static_cast<bool>(meta_type.find_method("set_value")));

  REQUIRE(caors::serialization::contains(meta_type));
  const std::optional<caors::serialization::type_schema> schema =
      caors::serialization::find(meta_type);
  REQUIRE(schema.has_value());
  REQUIRE(schema->fields.size() == 1);
  CHECK(schema->fields[0].field_name == caors::core::string_name{"value"});

  const caors::objects::type object_type = caors::objects::find<registration_actor>();
  REQUIRE(object_type);
  CHECK(object_type.name() == caors::core::string_name{"caors.test.registration.actor"});
}
