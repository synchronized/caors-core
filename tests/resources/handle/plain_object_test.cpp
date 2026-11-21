// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/memory/tracking.h"
#include "caors/resources/io.h"
#include <catch2/catch_all.hpp>

namespace {
struct plain_base {
  int value = 42;
};
struct plain_object : plain_base {
  explicit plain_object(int& destroyed) : destroyed(&destroyed) {}
  ~plain_object() { ++*destroyed; }
  int* destroyed;
};
template <typename T>
concept has_guid = requires(const T& value) { value.guid(); };
static_assert(!has_guid<caors::resources::handle<plain_object>>);
static_assert(has_guid<caors::resources::handle<caors::resource>>);

class plain_loader final : public caors::resources::loader<plain_object> {
public:
  caors::error start(caors::resources::async_load_context,
                     caors::resources::load_completion<plain_object> completion) override {
    pending = std::move(completion);
    return {};
  }
  caors::resources::load_completion<plain_object> pending;
};
}

TEST_CASE("resources - plain object handles preserve concrete destruction and weak lifetime",
          "[resources][handle]") {
  int destroyed = 0;
  caors::tracking_resource memory(&caors::get_system_memory_resource());
  caors::resources::weak_handle<plain_base> weak;
  {
    auto owned = caors::resources::make_handle<plain_object>(&memory, destroyed);
    REQUIRE(owned);
    caors::resources::handle<plain_base> base = owned;
    weak = base;
    owned.reset();
    auto locked = weak.lock();
    REQUIRE(locked);
    CHECK(locked->value == 42);
    base.reset();
    CHECK(destroyed == 0);
    locked.reset();
    CHECK(destroyed == 1);
    CHECK(weak.expired());
  }
  weak.reset();
  CHECK(memory.statistics().bytes_in_use == 0);
}

TEST_CASE("resources - host loader can defer completion with a plain object",
          "[resources][handle]") {
  int destroyed = 0;
  int delivered = 0;
  plain_loader loader;
  auto receipt = caors::resources::detail::load_completion_access<plain_object>::make(
      nullptr, [&](caors::resources::load_result<plain_object> result) noexcept {
        if (result && result.value->value == 42)
          ++delivered;
      });
  auto state = caors::resources::detail::load_completion_access<plain_object>::receipt(receipt);
  auto status = loader.start({}, std::move(receipt));
  caors::resources::detail::load_completion_access<plain_object>::resolve(state, status);
  REQUIRE_FALSE(status);
  CHECK(delivered == 0);
  CHECK(
      loader.pending.complete({{}, caors::memory::make_unique<plain_object>(nullptr, destroyed)}));
  CHECK(delivered == 1);
  CHECK(destroyed == 1);
}

TEST_CASE("resources - adopting a plain object retains its allocator deleter",
          "[resources][handle]") {
  int destroyed = 0;
  caors::tracking_resource memory(&caors::get_system_memory_resource());
  {
    auto value = caors::memory::make_unique<plain_object>(&memory, destroyed);
    auto handle = caors::resources::adopt_handle(std::move(value), &memory);
    REQUIRE(handle);
    CHECK(handle->value == 42);
  }
  CHECK(destroyed == 1);
  CHECK(memory.statistics().bytes_in_use == 0);
}
