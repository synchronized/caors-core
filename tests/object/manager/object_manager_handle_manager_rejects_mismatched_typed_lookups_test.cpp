// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager rejects mismatched typed lookups", "[object]") {
  caors::objects::manager manager;

  const caors::error actor_status = register_object_test_type<managed_actor>();
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));
  REQUIRE(register_object_test_type<managed_other_actor>() == caors::error{});

  managed_actor* instance = manager.create<managed_actor>();
  REQUIRE(instance != nullptr);

  CHECK(manager.find<managed_other_actor>(instance->id()) == nullptr);
  CHECK_FALSE(manager.handle_of<managed_other_actor>(instance->id()));
  CHECK(manager.find(caors::objects::handle{instance->id(),
                                            managed_other_actor::static_type_id()}) == nullptr);
}
