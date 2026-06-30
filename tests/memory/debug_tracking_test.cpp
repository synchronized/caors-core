// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/containers.h"
#include "caors/memory.h"

#include <catch2/catch_all.hpp>

#include <cstring>

namespace {

struct failing_memory_state {
  int allocate_count = 0;
};

void* failed_allocate(std::size_t, std::size_t, void* user_data) {
  auto* state = static_cast<failing_memory_state*>(user_data);
  ++state->allocate_count;
  return nullptr;
}

void failed_deallocate(void*, std::size_t, std::size_t, void*) {}

} // namespace

TEST_CASE("memory debug tracking - records live allocations", "[memory]") {
  caors::debug_tracking_resource debug(&caors::get_system_memory_resource());

  void* ptr = debug.allocate_debug(64, 32, "debug-buffer");

  REQUIRE(ptr != nullptr);
  CHECK(debug.has_leaks());
  CHECK(debug.live_allocation_count() == 1);
  CHECK(debug.live_bytes() == 64);

  std::size_t visited_count = 0;
  debug.for_each_live_allocation(
      [](const caors::memory_debug_record& record, void* user_data) {
        auto* count = static_cast<std::size_t*>(user_data);
        if (record.ptr != nullptr) {
          ++(*count);
        }
      },
      &visited_count);
  CHECK(visited_count == 1);

  const auto records = caors::collect_live_allocations(debug);
  REQUIRE(records.size() == 1);
  CHECK(records[0].ptr == ptr);
  CHECK(records[0].size == 64);
  CHECK(records[0].alignment == 32);
  REQUIRE(records[0].label != nullptr);
  CHECK(std::strcmp(records[0].label, "debug-buffer") == 0);
  REQUIRE(records[0].file_name != nullptr);
  CHECK(records[0].line > 0);

  auto stats = debug.debug_statistics();
  CHECK(stats.memory.allocation_count == 1);
  CHECK(stats.memory.bytes_in_use == 64);
  CHECK(stats.live_allocation_count == 1);
  CHECK(stats.invalid_deallocation_count == 0);
  CHECK(stats.mismatched_deallocation_count == 0);

  debug.deallocate(ptr, 64, 32);

  stats = debug.debug_statistics();
  CHECK(stats.memory.deallocation_count == 1);
  CHECK(stats.memory.bytes_in_use == 0);
  CHECK(stats.live_allocation_count == 0);
  CHECK(!debug.has_leaks());
  CHECK(caors::collect_live_allocations(debug).empty());
}

TEST_CASE("memory debug tracking - detects invalid deallocation", "[memory]") {
  caors::debug_tracking_resource debug(&caors::get_system_memory_resource());

  void* ptr = debug.allocate(32, 16);
  debug.deallocate(ptr, 32, 16);
  debug.deallocate(ptr, 32, 16);

  const auto stats = debug.debug_statistics();
  CHECK(stats.memory.allocation_count == 1);
  CHECK(stats.memory.deallocation_count == 1);
  CHECK(stats.invalid_deallocation_count == 1);
  CHECK(stats.live_allocation_count == 0);
}

TEST_CASE("memory debug tracking - detects deallocation mismatches", "[memory]") {
  caors::debug_tracking_resource debug(&caors::get_system_memory_resource());

  void* ptr = debug.allocate(32, 16);
  debug.deallocate(ptr, 64, 16);

  const auto stats = debug.debug_statistics();
  CHECK(stats.memory.deallocation_count == 1);
  CHECK(stats.mismatched_deallocation_count == 1);
  CHECK(stats.live_allocation_count == 0);
  CHECK(stats.memory.bytes_in_use == 0);
}

TEST_CASE("memory debug tracking - keeps failed allocations out of live records", "[memory]") {
  failing_memory_state state;
  caors::callback_memory_resource failing(&failed_allocate, &failed_deallocate, &state);
  caors::debug_tracking_resource debug(&failing);

  CHECK(debug.try_allocate_debug(48, 32, "failed") == nullptr);
  CHECK(debug.live_allocation_count() == 0);
  CHECK(caors::collect_live_allocations(debug).empty());
  CHECK(debug.debug_statistics().memory.allocation_count == 0);
  CHECK(state.allocate_count == 1);
}
