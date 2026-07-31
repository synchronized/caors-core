// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：debug_tracking_memory_debug_tracking_detects_deallocation_mismatches_test.cpp

#include "debug_tracking_test_support.h"

#include <limits>

using namespace caors::tests::debug_tracking;
TEST_CASE("memory debug tracking - detects deallocation mismatches", "[memory]") {
  caors::memory::debug_tracking_resource debug(&caors::get_system_memory_resource());

  void* ptr = debug.allocate(32, 16);
  debug.deallocate(ptr, 64, 16);

  const auto stats = debug.debug_statistics();
  CHECK(stats.memory.deallocation_count == 1);
  CHECK(stats.mismatched_deallocation_count == 1);
  CHECK(stats.live_allocation_count == 0);
  CHECK(stats.memory.bytes_in_use == 0);
}

// 原测试文件：debug_tracking_memory_debug_tracking_detects_invalid_deallocation_test.cpp

#include "debug_tracking_test_support.h"

using namespace caors::tests::debug_tracking;
TEST_CASE("memory debug tracking - detects invalid deallocation", "[memory]") {
  caors::memory::debug_tracking_resource debug(&caors::get_system_memory_resource());

  void* ptr = debug.allocate(32, 16);
  debug.deallocate(ptr, 32, 16);
  debug.deallocate(ptr, 32, 16);

  const auto stats = debug.debug_statistics();
  CHECK(stats.memory.allocation_count == 1);
  CHECK(stats.memory.deallocation_count == 1);
  CHECK(stats.invalid_deallocation_count == 1);
  CHECK(stats.live_allocation_count == 0);
}

// 原测试文件：debug_tracking_memory_debug_tracking_keeps_failed_allocations_out_of_live_records_test.cpp

#include "debug_tracking_test_support.h"

using namespace caors::tests::debug_tracking;
TEST_CASE("memory debug tracking - keeps failed allocations out of live records", "[memory]") {
  failing_memory_state state;
  caors::memory::callback_memory_resource failing(&failed_allocate, &failed_deallocate, &state);
  caors::memory::debug_tracking_resource debug(&failing);

  CHECK(debug.try_allocate_debug(48, 32, "failed") == nullptr);
  CHECK(debug.live_allocation_count() == 0);
  CHECK(caors::memory::collect_live_allocations(debug).empty());
  CHECK(debug.debug_statistics().memory.allocation_count == 0);
  CHECK(state.allocate_count == 1);
}

// 原测试文件：debug_tracking_memory_debug_tracking_records_live_allocations_test.cpp

#include "debug_tracking_test_support.h"

using namespace caors::tests::debug_tracking;
TEST_CASE("memory debug tracking - records live allocations", "[memory]") {
  caors::memory::debug_tracking_resource debug(&caors::get_system_memory_resource());

  void* ptr = debug.allocate_debug(64, 32, "debug-buffer");

  REQUIRE(ptr != nullptr);
  CHECK(debug.has_leaks());
  CHECK(debug.live_allocation_count() == 1);
  CHECK(debug.live_bytes() == 64);

  std::size_t visited_count = 0;
  debug.for_each_live_allocation(
      [](const caors::memory::memory_debug_record& record, void* user_data) {
        auto* count = static_cast<std::size_t*>(user_data);
        if (record.ptr != nullptr) {
          ++(*count);
        }
      },
      &visited_count);
  CHECK(visited_count == 1);

  const auto records = caors::memory::collect_live_allocations(debug);
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
  CHECK(caors::memory::collect_live_allocations(debug).empty());
}

TEST_CASE("memory debug tracking - fills payload and preserves upstream allocation metadata",
          "[memory]") {
  retained_memory_state state;
  caors::memory::callback_memory_resource upstream(&retained_allocate, &retained_deallocate,
                                                   &state);
  caors::memory::memory_debug_options options;
  options.guard_size = 8;
  options.allocation_fill_pattern = std::byte{0xA5};
  options.deallocation_fill_pattern = std::byte{0x5A};
  caors::memory::debug_tracking_resource debug(&upstream, options);

  void* ptr = debug.allocate(24, 32);

  REQUIRE(ptr != nullptr);
  CHECK(reinterpret_cast<std::uintptr_t>(ptr) % 32 == 0);
  CHECK(state.allocated_size == 24 + 16 + 31);
  CHECK(state.allocated_alignment == 32);

  const auto* payload = static_cast<const std::byte*>(ptr);
  for (std::size_t index = 0; index < 24; ++index) {
    CHECK(payload[index] == std::byte{0xA5});
  }

  debug.deallocate(ptr, 24, 32);

  CHECK(state.deallocate_count == 1);
  CHECK(state.deallocated_ptr == state.allocated_ptr);
  CHECK(state.deallocated_size == state.allocated_size);
  CHECK(state.deallocated_alignment == state.allocated_alignment);
  for (std::size_t index = 0; index < 24; ++index) {
    CHECK(payload[index] == std::byte{0x5A});
  }
}

TEST_CASE("memory debug tracking - detects front and back guard corruption", "[memory]") {
  retained_memory_state state;
  caors::memory::callback_memory_resource upstream(&retained_allocate, &retained_deallocate,
                                                   &state);
  caors::memory::memory_debug_options options;
  options.guard_size = 8;
  caors::memory::debug_tracking_resource debug(&upstream, options);
  violation_capture capture{&debug};
  debug.set_violation_callback(&capture_violation, &capture);

  auto* ptr = static_cast<std::byte*>(debug.allocate(16, 16));
  REQUIRE(ptr != nullptr);

  *(reinterpret_cast<std::byte*>(reinterpret_cast<std::uintptr_t>(ptr) - 1)) = std::byte{0};
  *(reinterpret_cast<std::byte*>(reinterpret_cast<std::uintptr_t>(ptr) + 16)) = std::byte{0};
  debug.deallocate(ptr, 16, 16);

  const auto stats = debug.debug_statistics();
  CHECK(stats.front_guard_corruption_count == 1);
  CHECK(stats.back_guard_corruption_count == 1);
  REQUIRE(capture.count == 2);
  CHECK(capture.kinds[0] == caors::memory::memory_debug_violation_kind::front_guard_corruption);
  CHECK(capture.kinds[1] == caors::memory::memory_debug_violation_kind::back_guard_corruption);
  CHECK(capture.reentered);
  CHECK(state.deallocate_count == 1);
}

TEST_CASE("memory debug tracking - guard zero keeps direct upstream layout", "[memory]") {
  retained_memory_state state;
  caors::memory::callback_memory_resource upstream(&retained_allocate, &retained_deallocate,
                                                   &state);
  caors::memory::memory_debug_options options;
  options.guard_size = 0;
  options.allocation_fill_pattern = std::byte{0x3C};
  options.deallocation_fill_pattern = std::byte{0xC3};
  caors::memory::debug_tracking_resource debug(&upstream, options);

  void* ptr = debug.allocate(32, 16);

  REQUIRE(ptr != nullptr);
  CHECK(ptr == state.allocated_ptr);
  CHECK(state.allocated_size == 32);
  CHECK(state.allocated_alignment == 16);
  const auto* payload = static_cast<const std::byte*>(ptr);
  for (std::size_t index = 0; index < 32; ++index) {
    CHECK(payload[index] == std::byte{0x3C});
  }

  debug.deallocate(ptr, 32, 16);
  CHECK(state.deallocated_ptr == ptr);
  CHECK(state.deallocated_size == 32);
  for (std::size_t index = 0; index < 32; ++index) {
    CHECK(payload[index] == std::byte{0xC3});
  }
  CHECK(debug.debug_statistics().front_guard_corruption_count == 0);
  CHECK(debug.debug_statistics().back_guard_corruption_count == 0);
}

TEST_CASE("memory debug tracking - normalizes zero size with guarded extended alignment",
          "[memory]") {
  retained_memory_state state;
  caors::memory::callback_memory_resource upstream(&retained_allocate, &retained_deallocate,
                                                   &state);
  caors::memory::debug_tracking_resource debug(&upstream);

  void* ptr = debug.allocate(0, 64);

  REQUIRE(ptr != nullptr);
  CHECK(reinterpret_cast<std::uintptr_t>(ptr) % 64 == 0);
  CHECK(state.allocated_size == 1 + 32 + 63);
  CHECK(debug.live_bytes() == 1);

  const auto records = caors::memory::collect_live_allocations(debug);
  REQUIRE(records.size() == 1);
  CHECK(records[0].size == 1);
  CHECK(records[0].alignment == 64);

  debug.deallocate(ptr, 0, 64);
  CHECK(debug.debug_statistics().mismatched_deallocation_count == 0);
  CHECK(state.deallocated_size == state.allocated_size);
}

TEST_CASE("memory debug tracking - rejects guard layout overflow before upstream allocation",
          "[memory]") {
  retained_memory_state state;
  caors::memory::callback_memory_resource upstream(&retained_allocate, &retained_deallocate,
                                                   &state);
  caors::memory::memory_debug_options options;
  options.guard_size = std::numeric_limits<std::size_t>::max();
  caors::memory::debug_tracking_resource debug(&upstream, options);

  CHECK(debug.try_allocate(16, 16) == nullptr);
  CHECK(state.allocate_count == 0);
  CHECK(debug.live_allocation_count() == 0);
}

TEST_CASE("memory debug tracking - reports invalid and mismatched deallocation", "[memory]") {
  retained_memory_state state;
  caors::memory::callback_memory_resource upstream(&retained_allocate, &retained_deallocate,
                                                   &state);
  caors::memory::debug_tracking_resource debug(&upstream);
  violation_capture capture;
  debug.set_violation_callback(&capture_violation, &capture);

  void* ptr = debug.allocate(16, 16);
  debug.deallocate(ptr, 32, 16);
  debug.deallocate(ptr, 16, 16);

  const auto stats = debug.debug_statistics();
  CHECK(stats.mismatched_deallocation_count == 1);
  CHECK(stats.invalid_deallocation_count == 1);
  REQUIRE(capture.count == 2);
  CHECK(capture.kinds[0] == caors::memory::memory_debug_violation_kind::mismatched_deallocation);
  CHECK(capture.kinds[1] == caors::memory::memory_debug_violation_kind::invalid_deallocation);
}
