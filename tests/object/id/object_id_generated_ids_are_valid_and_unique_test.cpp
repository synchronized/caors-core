// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - generated ids are valid and unique", "[object]") {
  const caors::object_id generated = caors::objects::generate_object_id();
  runtime_actor first;
  runtime_actor second;

  CHECK(static_cast<bool>(generated));
  CHECK(static_cast<bool>(first.id()));
  CHECK(static_cast<bool>(second.id()));
  CHECK(generated != first.id());
  CHECK(generated != second.id());
  CHECK(first.id() != second.id());
}
