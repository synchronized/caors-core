// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <catch2/catch_all.hpp>

namespace {

struct freeze_actor {};

struct macro_actor {};

struct late_actor {};

struct freeze_component {
  bool enabled = false;
};

struct late_component {
  bool enabled = false;
};

[[nodiscard]] constexpr caors::error meta_error(caors::errc code) noexcept {
  return {caors::error_domain::meta, code};
}

} // namespace

CAORS_META_REGISTRATION(register_freeze_macro_actor) {
  const caors::meta_type type = caors::meta<macro_actor>("caors.test.freeze.macro_actor").get();
  return type.valid() ? caors::error{} : meta_error(caors::errc::invalid_type);
}

TEST_CASE("freeze - meta registry becomes read only", "[freeze]") {
  REQUIRE_FALSE(caors::is_meta_registry_frozen());
  CHECK_FALSE(caors::type::get<macro_actor>());

  const caors::meta_type type = caors::meta<freeze_actor>("caors.test.freeze.actor").get();
  REQUIRE(type.valid());
  REQUIRE(caors::get_meta_registry().emplace<freeze_component>(type.id(), true) == caors::error{});

  CHECK(caors::freeze_meta_registry() == caors::error{});
  CHECK(caors::is_meta_registry_frozen());
  CHECK(caors::freeze_meta_registry() == caors::error{});
  CHECK(caors::type::get<macro_actor>().valid());

  CHECK(caors::register_type<late_actor>("caors.test.freeze.late") ==
        meta_error(caors::errc::frozen));
  CHECK(caors::get_meta_registry().emplace<late_component>(type.id(), true) ==
        meta_error(caors::errc::frozen));

  CHECK_FALSE(caors::type_id_of<late_actor>());
  CHECK(caors::type::get<freeze_actor>().id() == type.id());
}
