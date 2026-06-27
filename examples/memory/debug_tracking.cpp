// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"

#include <iostream>

int main() {
  caors::debug_tracking_resource debug(&caors::get_system_memory_resource());

  void* buffer = debug.allocate_debug(128, 64, "temporary-buffer");

  for (const caors::memory_debug_record& record : debug.live_allocations()) {
    std::cout << "live allocation: " << record.size << " bytes";
    if (record.label != nullptr) {
      std::cout << ", label: " << record.label;
    }
    std::cout << '\n';
  }

  debug.deallocate(buffer, 128, 64);

  const caors::memory_debug_statistics stats = debug.debug_statistics();
  std::cout << "invalid deallocations: " << stats.invalid_deallocation_count << '\n';
  std::cout << "mismatched deallocations: " << stats.mismatched_deallocation_count << '\n';

  return 0;
}
