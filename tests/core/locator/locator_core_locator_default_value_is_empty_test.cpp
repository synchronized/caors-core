// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "locator_test_support.h"

using namespace caors::tests::core_locator;
TEST_CASE("core locator - default value is empty", "[core]") {
  using service_locator = caors::core::locator<test_service>;
  service_locator::reset();

  CHECK_FALSE(service_locator::has_value());
  CHECK(service_locator::get() == nullptr);
}
