// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_type_registry_test_support.h"

using namespace caors::tests::meta_type_registry;
TEST_CASE("meta type - global registry reports registration errors", "[meta]") {
  CHECK(caors::meta::type_<error_child_type>("caors.test.type.missing_parent_child")
            .parent(caors::type_id{std::numeric_limits<std::uint64_t>::max()})
            .result() == meta_error(caors::errc::missing_parent));

  REQUIRE(caors::meta::type_<error_parent_type>("caors.test.type.error_parent").result() ==
          caors::error{});

  CHECK(caors::meta::type_<error_child_type>("caors.test.type.error_parent").result() ==
        meta_error(caors::errc::duplicate_name));

  const auto guid = caors::guid::parse("c9e7a3da-2cc0-4048-97ef-d9327f71b09e");
  REQUIRE(guid.has_value());
  REQUIRE(caors::meta::type_<duplicate_guid_owner_type>("caors.test.type.guid_owner")
              .guid(caors::type_guid{*guid})
              .result() == caors::error{});

  CHECK(caors::meta::type_<duplicate_guid_type>("caors.test.type.duplicate_guid")
            .guid(caors::type_guid{*guid})
            .result() == meta_error(caors::errc::duplicate_guid));
}
