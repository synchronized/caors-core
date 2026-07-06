// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/locator.h"

#include <catch2/catch_all.hpp>

namespace {

struct test_service {
  int value = 0;
};

} // namespace

TEST_CASE("core locator - default value is empty", "[core]") {
  using service_locator = caors::core::locator<test_service>;
  service_locator::reset();

  CHECK_FALSE(service_locator::has_value());
  CHECK(service_locator::get() == nullptr);
}

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
