// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/error.h"
#include "caors/meta.h"
#include "caors/object.h"
#include "caors/reflection.h"

#include <catch2/catch_all.hpp>

#include <system_error>

TEST_CASE("error - errc converts to std error code", "[error]") {
  const std::error_code code = caors::errc::invalid_type;

  CHECK(code);
  CHECK(code.value() == static_cast<int>(caors::errc::invalid_type));
  CHECK(code.category() == caors::error_category());
  CHECK(code.message() == "invalid type");
}

TEST_CASE("error - lightweight status keeps domain and code", "[error]") {
  constexpr caors::error ok = caors::error::ok();
  CHECK(ok.succeeded());
  CHECK_FALSE(ok.failed());
  CHECK(ok.domain() == caors::error_domain::none);
  CHECK(ok.value() == caors::errc::success);

  constexpr caors::error failed{caors::error_domain::serialization, caors::errc::unsupported};
  CHECK(failed.failed());
  CHECK_FALSE(failed.succeeded());
  CHECK(failed.domain() == caors::error_domain::serialization);
  CHECK(failed.value() == caors::errc::unsupported);
  CHECK(failed.code().message() == "unsupported");
}

TEST_CASE("error - meta results map to common errors", "[error]") {
  CHECK(caors::to_error(caors::type_register_result::success).succeeded());

  const caors::error duplicate_name = caors::to_error(caors::type_register_result::duplicate_name);
  CHECK(duplicate_name.domain() == caors::error_domain::meta);
  CHECK(duplicate_name.value() == caors::errc::duplicate_name);

  const caors::error duplicate_component =
      caors::to_error(caors::meta_component_result::duplicate_component);
  CHECK(duplicate_component.domain() == caors::error_domain::meta);
  CHECK(duplicate_component.value() == caors::errc::duplicate_component);
}

TEST_CASE("error - object and reflection results map to their domains", "[error]") {
  const caors::error object_error = caors::to_error(caors::object_register_result::missing_parent);
  CHECK(object_error.domain() == caors::error_domain::object);
  CHECK(object_error.value() == caors::errc::missing_parent);

  const caors::error reflection_error =
      caors::to_error(caors::reflection_register_result::invalid_field);
  CHECK(reflection_error.domain() == caors::error_domain::reflection);
  CHECK(reflection_error.value() == caors::errc::invalid_field);
}
