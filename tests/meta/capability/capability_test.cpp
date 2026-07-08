// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <catch2/catch_all.hpp>

namespace {

struct runtime_capability {
  int value = 0;
};

struct runtime_capability_type {};

struct static_capability_data {
  int value = 0;
};

struct static_capability_type {};

} // namespace

namespace caors::meta::capabilities {

template <> struct static_capability<static_capability_data, static_capability_type> {
  [[nodiscard]] static const static_capability_data* get() {
    static const static_capability_data data{42};
    return &data;
  }
};

} // namespace caors::meta::capabilities

TEST_CASE("meta - capability registry stores runtime capabilities by type id", "[meta]") {
  static const runtime_capability capability{7};
  const caors::type_id id = caors::meta::id_of<runtime_capability_type>();

  CHECK(caors::meta::capabilities::find<runtime_capability>(id) == nullptr);
  REQUIRE(caors::meta::capabilities::register_for<runtime_capability>(id, &capability).succeeded());

  const runtime_capability* found = caors::meta::capabilities::find<runtime_capability>(id);
  REQUIRE(found != nullptr);
  CHECK(found == &capability);
  CHECK(found->value == 7);
  CHECK(caors::meta::capabilities::contains<runtime_capability>(id));
}

TEST_CASE("meta - capability static provider can be registered into runtime index", "[meta]") {
  const static_capability_data* capability =
      caors::meta::capabilities::get<static_capability_data, static_capability_type>();
  REQUIRE(capability != nullptr);
  CHECK(capability->value == 42);

  REQUIRE(caors::meta::capabilities::register_for<static_capability_data, static_capability_type>()
              .succeeded());

  const static_capability_data* found = caors::meta::capabilities::find<static_capability_data>(
      caors::meta::id_of<static_capability_type>());
  REQUIRE(found != nullptr);
  CHECK(found == capability);
}
