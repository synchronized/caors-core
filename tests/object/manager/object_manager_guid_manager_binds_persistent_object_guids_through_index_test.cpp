// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager binds persistent object guids through index", "[object]") {
  caors::objects::object_index index;
  caors::objects::manager manager(index);

  const caors::error actor_status = register_object_test_type<managed_actor>();
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));

  managed_actor* instance = manager.create_with_guid<managed_actor>(test_object_guid_a);
  REQUIRE(instance != nullptr);

  const caors::object_id id = instance->id();
  CHECK(index.find(test_object_guid_a) == id);
  CHECK(index.guid_of(id) == test_object_guid_a);
  CHECK(manager.guid_of(id) == test_object_guid_a);
  CHECK(manager.find(test_object_guid_a) == instance);
  CHECK_FALSE(manager.bind_guid(id, test_object_guid_b));

  CHECK(manager.destroy(id));
  CHECK(index.empty());
  CHECK(manager.find(test_object_guid_a) == nullptr);
}
