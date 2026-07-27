// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/registration.h"

#include "caors/meta/method.h"
#include "caors/meta/property.h"

#include <catch2/catch_all.hpp>

namespace {

struct registration_actor final : caors::object {
  int value = 0;

  void set_value(int next) { value = next; }
};

} // namespace

TEST_CASE("registration - type builder registers meta serialization and object together",
          "[registration]") {
  const caors::meta::type fallback = caors::meta::resolve<registration_actor>();
  REQUIRE(fallback);
  REQUIRE_FALSE(fallback.registered());
  const caors::meta::value_storage_ops* fallback_value_ops =
      fallback.get_raw_ops<caors::meta::value_storage_ops>();
  REQUIRE(fallback_value_ops != nullptr);

  const caors::error status =
      caors::registration::type_<registration_actor>("caors.test.registration.actor")
          .meta()
          .property("value", &registration_actor::value)
          .method("set_value", &registration_actor::set_value)
          .serialization()
          .field("value")
          .commit();

  REQUIRE(status == caors::error{});

  const caors::meta::type meta_type = caors::meta::find<registration_actor>();
  REQUIRE(meta_type.registered());
  CHECK(meta_type.name() == caors::core::string_name{"caors.test.registration.actor"});
  CHECK(meta_type.kind() == fallback.kind());
  const caors::meta::value_storage_ops* registered_value_ops =
      meta_type.get_raw_ops<caors::meta::value_storage_ops>();
  REQUIRE(registered_value_ops != nullptr);
  CHECK(registered_value_ops->layout.size == fallback_value_ops->layout.size);
  CHECK(registered_value_ops->layout.alignment == fallback_value_ops->layout.alignment);
  CHECK(registered_value_ops->ops.default_construct == fallback_value_ops->ops.default_construct);
  CHECK(registered_value_ops->ops.destroy == fallback_value_ops->ops.destroy);
  CHECK(static_cast<bool>(meta_type.find_property("value")));
  CHECK(static_cast<bool>(meta_type.find_method("set_value")));

  REQUIRE(caors::serialization::contains(meta_type));
  const caors::serialization::schema_view schema = caors::serialization::find_schema(meta_type);
  REQUIRE(schema);
  REQUIRE(schema.field_count() == 1);
  const caors::serialization::field_schema* field = schema.field_at(0);
  REQUIRE(field != nullptr);
  CHECK(field->field_name == caors::core::string_name{"value"});
  CHECK(static_cast<bool>(meta_type.find_property(field->field_name)));

  const caors::meta::type object_type = caors::objects::find<registration_actor>();
  REQUIRE(object_type);
  CHECK(object_type.name() == caors::core::string_name{"caors.test.registration.actor"});
}

TEST_CASE("registration - duplicate type builder commit does not mutate registered data",
          "[registration]") {
  struct duplicate_actor {
    int first = 0;
    int second = 0;
  };

  REQUIRE(caors::registration::type_<duplicate_actor>("caors.test.registration.duplicate")
              .meta()
              .property("first", &duplicate_actor::first)
              .serialization()
              .field("first")
              .commit() == caors::error{});

  const caors::meta::type old_meta_type = caors::meta::find<duplicate_actor>();
  REQUIRE(old_meta_type.registered());
  REQUIRE(static_cast<bool>(old_meta_type.find_property("first")));
  REQUIRE_FALSE(static_cast<bool>(old_meta_type.find_property("second")));

  const caors::error duplicate_status =
      caors::registration::type_<duplicate_actor>("caors.test.registration.duplicate")
          .meta()
          .property("second", &duplicate_actor::second)
          .serialization()
          .field("second")
          .commit();
  REQUIRE(duplicate_status);
  CHECK(duplicate_status.value() == caors::errc::duplicate_id);

  const caors::meta::type meta_type = caors::meta::find<duplicate_actor>();
  REQUIRE(meta_type.registered());
  CHECK(static_cast<bool>(meta_type.find_property("first")));
  CHECK_FALSE(static_cast<bool>(meta_type.find_property("second")));
  CHECK(static_cast<bool>(old_meta_type.find_property("first")));
  CHECK_FALSE(static_cast<bool>(old_meta_type.find_property("second")));

  const caors::serialization::schema_view schema =
      meta_type.get_capability<caors::serialization::schema_capability>();
  REQUIRE(schema);
  REQUIRE(schema.field_count() == 1);
  const caors::serialization::field_schema* field = schema.field_at(0);
  REQUIRE(field != nullptr);
  CHECK(field->field_name == caors::core::string_name{"first"});
}
