// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <catch2/catch_all.hpp>

namespace {

struct init_actor {};

struct macro_actor {};

struct late_actor {};

struct init_component {
  bool enabled = false;
};

struct late_component {
  bool enabled = false;
};

[[nodiscard]] constexpr caors::error meta_error(caors::errc code) noexcept {
  return {caors::error_domain::meta, code};
}

} // namespace

CAORS_META_REGISTRATION(register_init_macro_actor) {
  const caors::meta::type type =
      caors::meta::class_<macro_actor>("caors.test.init.macro_actor").get();
  return type.valid() ? caors::error{} : meta_error(caors::errc::invalid_type);
}

TEST_CASE("registration - meta registry init runs registrations without becoming read only",
          "[registration]") {
  CHECK_FALSE(caors::meta::find<macro_actor>());

  const caors::meta::type type = caors::meta::class_<init_actor>("caors.test.init.actor").get();
  REQUIRE(type.valid());
  REQUIRE(caors::meta::get_registry().emplace_component<init_component>(type.id(), true) ==
          caors::error{});

  CHECK(caors::meta::init_registry() == caors::error{});
  CHECK(caors::meta::find<macro_actor>().registered());

  const caors::meta::type late_type = caors::meta::class_<late_actor>("caors.test.init.late").get();
  REQUIRE(late_type.registered());
  CHECK(caors::meta::get_registry().emplace_component<late_component>(type.id(), true) ==
        caors::error{});

  CHECK(caors::meta::find<late_actor>().id() == late_type.id());
  CHECK(caors::meta::find<init_actor>().id() == type.id());
}
