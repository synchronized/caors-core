// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "memory_block_test_support.h"

using namespace caors::tests::memory_block;
TEST_CASE("memory - memory block survives default resource changes", "[memory]") {
  caors::tracking_resource first(&caors::get_system_memory_resource());
  caors::tracking_resource second(&caors::get_system_memory_resource());
  caors::memory_resource* old_resource = caors::set_default_memory_resource(&first);

  caors::memory_block block = caors::allocate_block(40, 16);
  caors::memory_resource* replaced_resource = caors::set_default_memory_resource(&second);

  CHECK(replaced_resource == &first);

  block.reset();
  replaced_resource = caors::set_default_memory_resource(old_resource);

  CHECK(replaced_resource == &second);
  CHECK(first.statistics().allocation_count == 1);
  CHECK(first.statistics().deallocation_count == 1);
  CHECK(first.statistics().bytes_in_use == 0);
  CHECK(second.statistics().allocation_count == 0);
  CHECK(second.statistics().deallocation_count == 0);
}
