// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager rejects duplicate persistent guid bindings", "[object]") {
  caors::objects::object_index index;
  caors::objects::manager manager(index);

  const caors::error actor_status = register_object_test_type<managed_actor>();
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));

  managed_actor* first = manager.create_with_guid<managed_actor>(test_object_guid_a);
  REQUIRE(first != nullptr);
  const caors::object_id first_id = first->id();

  CHECK(manager.create_with_guid<managed_actor>(test_object_guid_a) == nullptr);
  CHECK(manager.size() == 1);
  CHECK(index.size() == 1);
  CHECK(manager.find(test_object_guid_a) == first);
  CHECK(index.find(test_object_guid_a) == first_id);

  managed_actor* second = manager.create<managed_actor>();
  REQUIRE(second != nullptr);
  CHECK_FALSE(manager.bind_guid(second->id(), test_object_guid_a));
  CHECK(manager.guid_of(second->id()) == caors::object_guid::nil());

  CHECK(manager.destroy(second));
  CHECK(manager.destroy(first_id));
  CHECK(manager.empty());
  CHECK(index.empty());
}
