// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_polymorphic_value_wrapper_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::polymorphic_value_wrapper;

TEST_CASE("serialization - polymorphic value wrappers reject unrelated dynamic types",
          "[serialization]") {
  require_polymorphic_value_metadata();

  caors::variant::object_type enemy_fields;
  enemy_fields.push_back(caors::value::variant_member{
      caors::string_name{"danger"},
      0,
      caors::variant::from(std::int64_t{9}),
  });

  caors::variant::object_type owner_envelope;
  owner_envelope.push_back(caors::value::variant_member{
      caors::string_name{"$type"},
      0,
      caors::variant::from(enemy_guid.value.string()),
  });
  owner_envelope.push_back(caors::value::variant_member{
      caors::string_name{"$value"},
      0,
      caors::variant::object(std::move(enemy_fields)),
  });

  caors::variant::object_type holder_fields;
  holder_fields.push_back(caors::value::variant_member{
      caors::string_name{"owner"},
      0,
      caors::variant::object(std::move(owner_envelope)),
  });

  polymorphic_value_holder decoded{};
  const caors::error status =
      caors::deserialize(decoded, caors::variant::object(std::move(holder_fields)));

  CHECK(status.failed());
  CHECK(status.value() == caors::errc::invalid_type);
  CHECK(decoded.owner == nullptr);
}
