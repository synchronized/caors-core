// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - registry reports registration errors", "[object]") {
  CHECK(caors::objects::class_<error_actor>("caors.test.object.missing_parent_child")
            .parent(caors::type_id{std::numeric_limits<std::uint64_t>::max()})
            .commit() == object_error(caors::errc::missing_parent));

  REQUIRE(register_object_test_type<error_actor>() == caors::error{});

  CHECK(register_object_test_type<error_player>(error_actor::object_type_name) ==
        object_error(caors::errc::duplicate_name));
}
