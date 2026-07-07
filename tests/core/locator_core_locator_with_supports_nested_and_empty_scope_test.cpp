// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "locator_test_support.h"

using namespace caors::tests::core_locator;
TEST_CASE("core locator - with supports nested and empty scope", "[core]") {
  using service_locator = caors::core::locator<test_service>;
  service_locator::reset();

  test_service outer{1};
  test_service inner{2};

  service_locator::with(&outer, [&] {
    CHECK(service_locator::get() == &outer);

    service_locator::with(&inner, [&] { CHECK(service_locator::get() == &inner); });
    CHECK(service_locator::get() == &outer);

    service_locator::with(nullptr, [] { CHECK_FALSE(service_locator::has_value()); });
    CHECK(service_locator::get() == &outer);
  });

  CHECK_FALSE(service_locator::has_value());
}
