// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <memory>
#include <variant>

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects empty non default constructible unique pointers",
          "[serialization]") {
  require_non_default_value_metadata();

  non_default_unique_ptr_actor actor{};

  const caors::error result = caors::deserialize_json(actor, R"({"value":{"value":7}})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_type);
  CHECK(actor.value == nullptr);
}
