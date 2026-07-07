// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - manager rejects duplicate delayed destruction requests", "[object]") {
  caors::objects::manager manager;
  delayed_destroy_actor::destroyed_count = 0;

  const caors::error register_status = register_object_test_type<delayed_destroy_actor>();
  REQUIRE((register_status == caors::error{} ||
           register_status == object_error(caors::errc::duplicate_id)));

  delayed_destroy_actor* instance = manager.create<delayed_destroy_actor>();
  REQUIRE(instance != nullptr);

  const caors::object_id id = instance->id();
  CHECK(manager.destroy_later(instance));
  CHECK_FALSE(manager.destroy_later(instance));
  CHECK_FALSE(manager.destroy_later(id));
  CHECK_FALSE(manager.destroy(instance));
  CHECK(manager.pending_destroy_count() == 1);
  CHECK(delayed_destroy_actor::destroyed_count == 0);

  manager.flush_destroy_queue();

  CHECK(delayed_destroy_actor::destroyed_count == 1);
  CHECK(manager.pending_destroy_count() == 0);
}
