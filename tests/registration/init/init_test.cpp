// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"
#include "caors/registration.h"

#include <catch2/catch_all.hpp>

namespace {

struct init_actor {};

struct macro_actor {};

struct late_actor {};

[[nodiscard]] constexpr caors::error meta_error(caors::errc code) noexcept {
  return {caors::error_domain::meta, code};
}

} // namespace

CAORS_META_REGISTRATION(register_init_macro_actor) {
  const caors::error status =
      caors::registration::type_<macro_actor>("caors.test.init.macro_actor").commit();
  if (status) {
    return status;
  }
  const caors::meta::type type = caors::meta::find<macro_actor>();
  return type.valid() ? caors::error{} : meta_error(caors::errc::invalid_type);
}

TEST_CASE("registration - meta registry init runs registrations without becoming read only",
          "[registration]") {
  CHECK_FALSE(caors::meta::find<macro_actor>());

  REQUIRE(caors::registration::type_<init_actor>("caors.test.init.actor").commit() ==
          caors::error{});
  const caors::meta::type type = caors::meta::find<init_actor>();
  REQUIRE(type.valid());

  CHECK(caors::meta::init_registry() == caors::error{});
  CHECK(caors::meta::find<macro_actor>().registered());

  REQUIRE(caors::registration::type_<late_actor>("caors.test.init.late")
              .meta()
              .metadata("enabled", true)
              .commit() == caors::error{});
  const caors::meta::type late_type = caors::meta::find<late_actor>();
  REQUIRE(late_type.registered());
  REQUIRE(late_type.find_metadata(caors::string_name{"enabled"}) != nullptr);
  CHECK(late_type.find_metadata(caors::string_name{"enabled"})->as_bool());

  CHECK(caors::meta::find<late_actor>().id() == late_type.id());
  CHECK(caors::meta::find<init_actor>().id() == type.id());
}
