// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "locator_test_support.h"

using namespace caors::tests::core_locator;
TEST_CASE("core locator - with restores previous service", "[core]") {
  using service_locator = caors::core::locator<test_service>;
  service_locator::reset();

  test_service outer{1};
  test_service inner{2};

  service_locator::set(&outer);

  const int result = service_locator::with(&inner, [] {
    REQUIRE(caors::locator<test_service>::get() != nullptr);
    return caors::locator<test_service>::get()->value;
  });

  CHECK(result == 2);
  CHECK(service_locator::get() == &outer);

  service_locator::reset();
}
