// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager rejects persistent guid operations without index", "[object]") {
  caors::objects::manager manager;

  const caors::error actor_status = register_object_test_type<managed_actor>();
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));

  CHECK(manager.create_with_guid<managed_actor>(test_object_guid_a) == nullptr);
  CHECK(manager.create_with_guid(managed_actor::static_type_id(), test_object_guid_a) == nullptr);

  managed_actor* instance = manager.create<managed_actor>();
  REQUIRE(instance != nullptr);

  CHECK_FALSE(manager.bind_guid(instance->id(), test_object_guid_a));
  CHECK(manager.guid_of(instance->id()) == caors::object_guid::nil());
  CHECK(manager.find(test_object_guid_a) == nullptr);

  CHECK(manager.destroy(instance));
  CHECK(manager.empty());
}
