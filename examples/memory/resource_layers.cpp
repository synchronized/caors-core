// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"

#include <iostream>
#include <vector>

struct entity {
  explicit entity(int id) : id(id) {}

  int id = 0;
};

int main() {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::fixed_block_pool pool(sizeof(entity), alignof(entity), 32, &tracking);

  entity* single = caors::make<entity>(&pool, 7);
  std::cout << "entity id: " << single->id << '\n';
  caors::destroy(single, &pool);

  std::vector<int, caors::allocator<int>> values{caors::allocator<int>(&tracking)};
  values.push_back(1);
  values.push_back(2);
  values.push_back(3);

  const caors::memory_statistics stats = tracking.statistics();
  std::cout << "allocations: " << stats.allocation_count << '\n';
  std::cout << "bytes allocated: " << stats.bytes_allocated << '\n';
  std::cout << "peak bytes in use: " << stats.peak_bytes_in_use << '\n';

  return 0;
}
