// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_object_ref_polymorphic_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ref_polymorphic;

TEST_CASE("serialization - polymorphic object refs reject unrelated dynamic types",
          "[serialization]") {
  require_polymorphic_object_ref_metadata();

  caors::variant::object_type enemy_fields;
  enemy_fields.push_back(caors::value::variant_member{
      caors::string_name{"danger"},
      0,
      caors::variant::from(std::int64_t{9}),
  });

  caors::variant::object_type envelope;
  envelope.push_back(caors::value::variant_member{
      caors::string_name{"$type"},
      0,
      caors::variant::from(polymorphic_object_ref_enemy_guid.value.string()),
  });
  envelope.push_back(caors::value::variant_member{
      caors::string_name{"$value"},
      0,
      caors::variant::object(std::move(enemy_fields)),
  });

  caors::variant::object_type holder_fields;
  holder_fields.push_back(caors::value::variant_member{
      caors::string_name{"owner"},
      0,
      caors::variant::object(std::move(envelope)),
  });

  polymorphic_object_ref_holder decoded{};
  const caors::error status =
      caors::deserialize(decoded, caors::variant::object(std::move(holder_fields)));

  CHECK(status.failed());
  CHECK(status.value() == caors::errc::invalid_type);
  CHECK(decoded.owner == nullptr);
}
