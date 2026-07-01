// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MEMORY_DEBUG_TOOLS_H_
#define CAORS_MEMORY_DEBUG_TOOLS_H_

#include "caors/core/containers.h"
#include "caors/memory/debug_tracking.h"

namespace caors {

/** @brief 将调试分配器的存活分配记录收集为 CAORS 动态数组。 */
[[nodiscard]] inline vector<memory_debug_record>
collect_live_allocations(const debug_tracking_resource& resource,
                         memory_resource* output_resource = nullptr) {
  vector<memory_debug_record> records{allocator<memory_debug_record>{output_resource}};
  records.reserve(resource.live_allocation_count());
  resource.for_each_live_allocation(
      [](const memory_debug_record& record, void* user_data) {
        auto* output = static_cast<vector<memory_debug_record>*>(user_data);
        output->push_back(record);
      },
      &records);
  return records;
}

} // namespace caors

#endif
