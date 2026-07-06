// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - serialized helpers reject invalid resource identity", "[resources]") {
  require_serialized_resource_metadata();

  const caors::serialization::variant_result parsed = caors::parse_json(R"({"width":256})");
  REQUIRE(parsed.succeeded());

  caors::serialization::variant_reader reader{parsed.value};
  caors::resource_load_result<serialized_resource> loaded =
      caors::load_serialized<serialized_resource>(caors::resource_guid::nil(), reader);

  REQUIRE(loaded.failed());
  CHECK(loaded.status.domain() == caors::error_domain::resources);
  CHECK(loaded.status.value() == caors::errc::invalid_argument);

  serialized_resource value{};
  caors::serialization::json_writer writer;
  const caors::error save_status = caors::save_serialized(value, writer);

  REQUIRE(save_status.failed());
  CHECK(save_status.domain() == caors::error_domain::resources);
  CHECK(save_status.value() == caors::errc::invalid_argument);
}
