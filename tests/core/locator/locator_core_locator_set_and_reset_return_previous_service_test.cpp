// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "locator_test_support.h"

using namespace caors::tests::core_locator;
TEST_CASE("core locator - set and reset return previous service", "[core]") {
  using service_locator = caors::core::locator<test_service>;
  service_locator::reset();

  test_service first{1};
  test_service second{2};

  CHECK(service_locator::set(&first) == nullptr);
  CHECK(service_locator::has_value());
  CHECK(service_locator::get() == &first);

  CHECK(service_locator::set(&second) == &first);
  CHECK(service_locator::get() == &second);

  CHECK(service_locator::reset() == &second);
  CHECK_FALSE(service_locator::has_value());
}
