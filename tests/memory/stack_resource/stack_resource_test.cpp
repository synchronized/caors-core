// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <cstdint>
#include <thread>

TEST_CASE("memory - stack resource rewinds nested scopes independently", "[memory][stack]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::stack_resource stack{64, &tracking};

  void* inner_address = nullptr;
  {
    caors::stack_scope outer{stack};
    auto* outer_value = static_cast<std::uint32_t*>(
        outer.resource()->allocate(sizeof(std::uint32_t), alignof(std::uint32_t)));
    *outer_value = 42;

    {
      caors::stack_scope inner{stack};
      inner_address = inner.resource()->allocate(16, 8);
      REQUIRE(inner_address != nullptr);
      CHECK(stack.active_scope_count() == 2);
    }

    CHECK(*outer_value == 42);
    CHECK(stack.active_scope_count() == 1);

    caors::stack_scope reused{stack};
    CHECK(reused.resource()->allocate(16, 8) == inner_address);
  }

  CHECK(stack.active_scope_count() == 0);
  stack.reset();
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("memory - stack resource satisfies over-aligned allocations", "[memory][stack]") {
  caors::stack_resource stack{32};
  caors::stack_scope scope{stack};

  void* allocation = scope.resource()->allocate(7, 128);

  REQUIRE(allocation != nullptr);
  CHECK(reinterpret_cast<std::uintptr_t>(allocation) % 128 == 0);
}

TEST_CASE("memory - stack resource releases large chunks on rewind", "[memory][stack]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::stack_resource stack{64, &tracking};

  std::size_t ordinary_bytes = 0;
  {
    caors::stack_scope outer{stack};
    REQUIRE(outer.resource()->allocate(16, 8) != nullptr);
    ordinary_bytes = tracking.statistics().bytes_in_use;

    {
      caors::stack_scope inner{stack};
      REQUIRE(inner.resource()->allocate(4096, 16) != nullptr);
      CHECK(tracking.statistics().bytes_in_use > ordinary_bytes);
    }

    CHECK(tracking.statistics().bytes_in_use == ordinary_bytes);
  }

  stack.trim();
  CHECK(tracking.statistics().bytes_in_use == ordinary_bytes);
  stack.reset();
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("memory - stack resource trim releases cached growth chunks", "[memory][stack]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::stack_resource stack{64, &tracking};

  {
    caors::stack_scope scope{stack};
    std::array<void*, 8> allocations{};
    for (void*& allocation : allocations) {
      allocation = scope.resource()->allocate(48, 8);
      REQUIRE(allocation != nullptr);
    }
  }

  const std::size_t before_trim = tracking.statistics().bytes_in_use;
  stack.trim();
  CHECK(tracking.statistics().bytes_in_use < before_trim);
  CHECK(tracking.statistics().bytes_in_use > 0);

  stack.reset();
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("memory - stack resource trim releases a large first chunk", "[memory][stack]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::stack_resource stack{64, &tracking};

  {
    caors::stack_scope scope{stack};
    REQUIRE(scope.resource()->allocate(4096, 16) != nullptr);
  }

  CHECK(tracking.statistics().bytes_in_use == 0);
  stack.trim();
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("memory - library temporary resource is isolated per thread", "[memory][stack]") {
  caors::stack_resource* first = nullptr;
  caors::stack_resource* second = nullptr;

  std::thread first_thread{[&first] { first = &caors::library_temporary_resource(); }};
  std::thread second_thread{[&second] { second = &caors::library_temporary_resource(); }};
  first_thread.join();
  second_thread.join();

  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  CHECK(first != second);
}

TEST_CASE("memory - thread local stack resource releases chunks at thread exit",
          "[memory][stack]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  bool allocated = false;
  bool observed_live_allocation = false;

  std::thread worker{[&] {
    thread_local caors::stack_resource scratch{64, &tracking};
    caors::stack_scope scope{scratch};
    allocated = scope.resource()->allocate(4096, 16) != nullptr;
    observed_live_allocation = tracking.statistics().bytes_in_use > 0;
  }};
  worker.join();

  REQUIRE(allocated);
  CHECK(observed_live_allocation);
  CHECK(tracking.statistics().allocation_count > 0);
  CHECK(tracking.statistics().bytes_in_use == 0);
  CHECK(tracking.statistics().allocation_count == tracking.statistics().deallocation_count);
}

TEST_CASE("memory - stack resource reports scoped allocation statistics", "[memory][stack]") {
  caors::stack_resource stack{64};

  CHECK(stack.statistics().bytes_in_use == 0);
  CHECK(stack.statistics().chunk_count == 0);

  {
    caors::stack_scope outer{stack};
    REQUIRE(outer.resource()->allocate(16, 8) != nullptr);

    const auto outer_statistics = stack.statistics();
    CHECK(outer_statistics.bytes_in_use >= 16);
    CHECK(outer_statistics.peak_bytes_in_use == outer_statistics.bytes_in_use);
    CHECK(outer_statistics.chunk_count == 1);
    CHECK(outer_statistics.peak_chunk_count == 1);
    CHECK(outer_statistics.mark_count == 1);

    {
      caors::stack_scope inner{stack};
      REQUIRE(inner.resource()->allocate(4096, 16) != nullptr);

      const auto inner_statistics = stack.statistics();
      CHECK(inner_statistics.bytes_in_use > outer_statistics.bytes_in_use);
      CHECK(inner_statistics.peak_bytes_in_use == inner_statistics.bytes_in_use);
      CHECK(inner_statistics.chunk_count == 2);
      CHECK(inner_statistics.peak_chunk_count == 2);
      CHECK(inner_statistics.large_allocation_count == 1);
      CHECK(inner_statistics.mark_count == 2);
    }

    const auto rewound_statistics = stack.statistics();
    CHECK(rewound_statistics.bytes_in_use == outer_statistics.bytes_in_use);
    CHECK(rewound_statistics.chunk_count == 1);
    CHECK(rewound_statistics.peak_chunk_count == 2);
    CHECK(rewound_statistics.rewind_count == 1);
  }

  const auto completed_statistics = stack.statistics();
  CHECK(completed_statistics.bytes_in_use == 0);
  CHECK(completed_statistics.chunk_count == 1);
  CHECK(completed_statistics.rewind_count == 2);

  stack.trim();
  CHECK(stack.statistics().trim_count == 1);
  CHECK(stack.statistics().chunk_count == 1);

  stack.reset();
  CHECK(stack.statistics().reset_count == 1);
  CHECK(stack.statistics().chunk_count == 0);
}

TEST_CASE("memory - stack resource resets counters without changing current watermarks",
          "[memory][stack]") {
  caors::stack_resource stack{64};
  caors::stack_scope scope{stack};
  REQUIRE(scope.resource()->allocate(24, 8) != nullptr);

  const auto before_reset = stack.statistics();
  stack.reset_statistics();
  const auto after_reset = stack.statistics();

  CHECK(after_reset.bytes_in_use == before_reset.bytes_in_use);
  CHECK(after_reset.peak_bytes_in_use == before_reset.bytes_in_use);
  CHECK(after_reset.chunk_count == before_reset.chunk_count);
  CHECK(after_reset.peak_chunk_count == before_reset.chunk_count);
  CHECK(after_reset.large_allocation_count == 0);
  CHECK(after_reset.mark_count == 0);
  CHECK(after_reset.rewind_count == 0);
  CHECK(after_reset.trim_count == 0);
  CHECK(after_reset.reset_count == 0);
}
