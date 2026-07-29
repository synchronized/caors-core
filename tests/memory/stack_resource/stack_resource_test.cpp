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
