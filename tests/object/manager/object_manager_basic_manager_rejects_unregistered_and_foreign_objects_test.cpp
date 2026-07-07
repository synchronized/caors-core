// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager rejects unregistered and foreign objects", "[object]") {
  caors::objects::manager manager;

  CHECK(manager.create<unregistered_managed_actor>() == nullptr);

  const caors::error actor_status = register_object_test_type<managed_actor>();
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));

  managed_actor external{};
  CHECK_FALSE(manager.destroy(&external));
  CHECK_FALSE(manager.contains(external.id()));
  CHECK(manager.find(external.id()) == nullptr);
}
