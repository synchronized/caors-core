// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/core/containers.h"
#include "caors/memory/tracking.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <concepts>
#include <cstdint>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

struct alignas(64) over_aligned_value {
  std::uint64_t value = 0;
};

struct lifetime_value {
  inline static int alive = 0;
  inline static int moved = 0;

  int value = 0;

  lifetime_value() { ++alive; }
  explicit lifetime_value(int source) : value(source) { ++alive; }
  lifetime_value(const lifetime_value& other) : value(other.value) { ++alive; }
  lifetime_value(lifetime_value&& other) noexcept : value(other.value) {
    other.value = -1;
    ++alive;
    ++moved;
  }
  lifetime_value& operator=(const lifetime_value&) = default;
  lifetime_value& operator=(lifetime_value&&) = default;
  ~lifetime_value() { --alive; }

  static void reset() noexcept {
    alive = 0;
    moved = 0;
  }
};

struct resource_aware_value {
  using allocator_type = caors::allocator<std::byte>;

  caors::memory_resource* resource = nullptr;

  resource_aware_value(std::allocator_arg_t, allocator_type allocator) noexcept
      : resource(allocator.resource()) {}
  resource_aware_value(std::allocator_arg_t, allocator_type allocator,
                       resource_aware_value&&) noexcept
      : resource(allocator.resource()) {}
  resource_aware_value(resource_aware_value&&) = delete;
  resource_aware_value(const resource_aware_value&) = delete;
};

#if CAORS_HAS_EXCEPTIONS
struct throwing_copy_value {
  inline static bool throw_on_copy = false;

  int value = 0;

  explicit throwing_copy_value(int source) : value(source) {}
  throwing_copy_value(const throwing_copy_value& other) : value(other.value) {
    if (throw_on_copy) {
      throw std::runtime_error{"复制失败"};
    }
  }
  throwing_copy_value(throwing_copy_value&& other) noexcept(false) : value(other.value) {
    other.value = -1;
  }
};

struct throwing_move_assignment_value {
  inline static int alive = 0;
  inline static bool throw_on_move_assignment = false;

  int value = 0;

  explicit throwing_move_assignment_value(int source) : value(source) { ++alive; }
  throwing_move_assignment_value(const throwing_move_assignment_value& other) : value(other.value) {
    ++alive;
  }
  throwing_move_assignment_value(throwing_move_assignment_value&& other) noexcept
      : value(other.value) {
    other.value = -1;
    ++alive;
  }
  throwing_move_assignment_value& operator=(throwing_move_assignment_value&& other) {
    if (throw_on_move_assignment) {
      throw std::runtime_error{"移动赋值失败"};
    }
    value = other.value;
    other.value = -1;
    return *this;
  }
  ~throwing_move_assignment_value() { --alive; }
};
#endif

} // namespace

static_assert(std::ranges::contiguous_range<caors::core::small_vector<int, 4>>);
static_assert(std::ranges::sized_range<caors::core::small_vector<int, 4>>);
static_assert(std::is_same_v<caors::small_vector<int, 4>, caors::core::small_vector<int, 4>>);
static_assert(caors::core::small_vector<int, 0>::inline_capacity() == 0);
static_assert(caors::core::small_vector<int, 4>::inline_capacity() == 4);

TEST_CASE("core small vector - empty container uses inline storage", "[core][small_vector]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::core::small_vector<int, 4> values{&tracking};

  CHECK(values.empty());
  CHECK(values.size() == 0);
  CHECK(values.capacity() == 4);
  CHECK(values.begin() == values.end());
  CHECK(values.data() != nullptr);
  CHECK(values.uses_inline_storage());
  CHECK(values.resource() == &tracking);
  CHECK(values.get_allocator().resource() == &tracking);
  CHECK(tracking.statistics().allocation_count == 0);
}

TEST_CASE("core small vector - zero inline capacity is valid", "[core][small_vector]") {
  caors::core::small_vector<int, 0> values;

  CHECK(values.empty());
  CHECK(values.capacity() == 0);
  CHECK(values.begin() == values.end());
  CHECK(values.uses_inline_storage());
}

TEST_CASE("core small vector - factory binds the requested resource", "[core][small_vector]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  auto values = caors::make_small_vector<int, 3>(&tracking);

  CHECK(values.resource() == &tracking);
  CHECK(values.capacity() == 3);
  CHECK(values.uses_inline_storage());
}

TEST_CASE("core small vector - stays inline until capacity is exceeded", "[core][small_vector]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};

  {
    caors::small_vector<int, 4> values{&tracking};
    values.push_back(1);
    values.emplace_back(2);
    values.push_back(3);
    values.emplace_back(4);

    CHECK(values.size() == 4);
    CHECK(values.capacity() == 4);
    CHECK(values.uses_inline_storage());
    CHECK(tracking.statistics().allocation_count == 0);

    values.push_back(5);

    CHECK(values.size() == 5);
    CHECK(values.capacity() >= 5);
    CHECK_FALSE(values.uses_inline_storage());
    CHECK(std::ranges::equal(values, std::initializer_list<int>{1, 2, 3, 4, 5}));
    CHECK(tracking.statistics().allocation_count == 1);
  }

  const auto statistics = tracking.statistics();
  CHECK(statistics.deallocation_count == statistics.allocation_count);
  CHECK(statistics.bytes_in_use == 0);
}

TEST_CASE("core small vector - zero inline capacity grows from the resource",
          "[core][small_vector]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::small_vector<int, 0> values{&tracking};

  values.emplace_back(42);

  REQUIRE(values.size() == 1);
  CHECK(values.front() == 42);
  CHECK_FALSE(values.uses_inline_storage());
  CHECK(tracking.statistics().allocation_count == 1);
}

TEST_CASE("core small vector - reserve relocates live elements", "[core][small_vector]") {
  lifetime_value::reset();
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};

  {
    caors::small_vector<lifetime_value, 2> values{&tracking};
    values.emplace_back(7);
    values.emplace_back(9);
    REQUIRE(lifetime_value::alive == 2);

    values.reserve(8);

    CHECK(values.capacity() == 8);
    CHECK(values[0].value == 7);
    CHECK(values[1].value == 9);
    CHECK(lifetime_value::alive == 2);
    CHECK(lifetime_value::moved == 2);
  }

  CHECK(lifetime_value::alive == 0);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("core small vector - clear pop and resize balance lifetimes", "[core][small_vector]") {
  lifetime_value::reset();

  {
    caors::small_vector<lifetime_value, 3> values;
    values.resize(2);
    CHECK(lifetime_value::alive == 2);

    values.emplace_back(3);
    values.pop_back();
    CHECK(lifetime_value::alive == 2);

    const lifetime_value fill{11};
    values.resize(5, fill);
    REQUIRE(values.size() == 5);
    CHECK(values[4].value == 11);
    CHECK(lifetime_value::alive == 6);

    values.resize(1);
    CHECK(lifetime_value::alive == 2);
    values.clear();
    CHECK(lifetime_value::alive == 1);
  }

  CHECK(lifetime_value::alive == 0);
}

TEST_CASE("core small vector - supports move-only elements", "[core][small_vector]") {
  caors::small_vector<std::unique_ptr<int>, 1> values;
  values.emplace_back(std::make_unique<int>(3));
  values.push_back(std::make_unique<int>(5));

  REQUIRE(values.size() == 2);
  CHECK(*values[0] == 3);
  CHECK(*values[1] == 5);
}

TEST_CASE("core small vector - growth preserves aliased push value", "[core][small_vector]") {
  caors::small_vector<std::string, 1> values;
  values.emplace_back("text");

  values.push_back(values.front());

  REQUIRE(values.size() == 2);
  CHECK(values[0] == "text");
  CHECK(values[1] == "text");
}

TEST_CASE("core small vector - nested allocator aware elements use container resource",
          "[core][small_vector]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::small_vector<resource_aware_value, 1> values{&tracking};

  values.emplace_back();
  values.emplace_back();

  REQUIRE(values.size() == 2);
  CHECK(values[0].resource == &tracking);
  CHECK(values[1].resource == &tracking);
}

TEST_CASE("core small vector - inline storage preserves element alignment",
          "[core][small_vector]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::core::small_vector<over_aligned_value, 2> values{&tracking};
  const auto inline_address = reinterpret_cast<std::uintptr_t>(values.data());

  CHECK(inline_address % alignof(over_aligned_value) == 0);
  CHECK(values.capacity() == 2);

  values.resize(3);
  const auto dynamic_address = reinterpret_cast<std::uintptr_t>(values.data());

  CHECK(dynamic_address % alignof(over_aligned_value) == 0);
  CHECK_FALSE(values.uses_inline_storage());
  CHECK(tracking.statistics().allocation_count == 1);
}

TEST_CASE("core small vector - default construction captures default resource",
          "[core][small_vector]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::memory_resource* previous = caors::set_default_memory_resource(&tracking);

  caors::core::small_vector<int, 2> values;
  caors::set_default_memory_resource(previous);

  CHECK(values.resource() == &tracking);
}

TEST_CASE("core small vector - copy construction preserves values and selects resource",
          "[core][small_vector]") {
  caors::tracking_resource source_resource{&caors::get_system_memory_resource()};
  caors::tracking_resource target_resource{&caors::get_system_memory_resource()};
  caors::small_vector<int, 2> source{&source_resource};
  source.push_back(1);
  source.push_back(2);
  source.push_back(3);

  caors::small_vector<int, 2> same_resource_copy{source};
  caors::small_vector<int, 2> selected_resource_copy{source, &target_resource};

  CHECK(same_resource_copy.resource() == &source_resource);
  CHECK(selected_resource_copy.resource() == &target_resource);
  CHECK(std::ranges::equal(same_resource_copy, source));
  CHECK(std::ranges::equal(selected_resource_copy, source));
  CHECK(same_resource_copy.data() != source.data());
  CHECK(selected_resource_copy.data() != source.data());
}

TEST_CASE("core small vector - move construction adopts compatible dynamic storage",
          "[core][small_vector]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::small_vector<int, 2> source{&tracking};
  source.push_back(4);
  source.push_back(5);
  source.push_back(6);
  const auto* dynamic_data = source.data();

  caors::small_vector<int, 2> target{std::move(source)};

  CHECK(target.data() == dynamic_data);
  CHECK(std::ranges::equal(target, std::initializer_list<int>{4, 5, 6}));
  CHECK(source.empty());
  CHECK(source.uses_inline_storage());
}

TEST_CASE("core small vector - move construction relocates for a different resource",
          "[core][small_vector]") {
  caors::tracking_resource source_resource{&caors::get_system_memory_resource()};
  caors::tracking_resource target_resource{&caors::get_system_memory_resource()};
  caors::small_vector<std::unique_ptr<int>, 1> source{&source_resource};
  source.emplace_back(std::make_unique<int>(7));
  source.emplace_back(std::make_unique<int>(9));
  const auto* source_data = source.data();

  caors::small_vector<std::unique_ptr<int>, 1> target{std::move(source), &target_resource};

  CHECK(target.resource() == &target_resource);
  CHECK(target.data() != source_data);
  REQUIRE(target.size() == 2);
  CHECK(*target[0] == 7);
  CHECK(*target[1] == 9);
  CHECK(source.empty());
}

TEST_CASE("core small vector - assignments follow resource propagation rules",
          "[core][small_vector]") {
  caors::tracking_resource source_resource{&caors::get_system_memory_resource()};
  caors::tracking_resource target_resource{&caors::get_system_memory_resource()};
  caors::small_vector<int, 1> source{&source_resource};
  source.push_back(2);
  source.push_back(4);

  caors::small_vector<int, 1> copied{&target_resource};
  copied = source;
  CHECK(copied.resource() == &target_resource);
  CHECK(std::ranges::equal(copied, source));

  caors::small_vector<int, 1> moved{&target_resource};
  moved = std::move(source);
  CHECK(moved.resource() == &source_resource);
  CHECK(std::ranges::equal(moved, std::initializer_list<int>{2, 4}));
  CHECK(source.empty());

  auto* copied_self = &copied;
  auto* moved_self = &moved;
  copied = *copied_self;
  moved = std::move(*moved_self);
  CHECK(std::ranges::equal(copied, std::initializer_list<int>{2, 4}));
  CHECK(std::ranges::equal(moved, std::initializer_list<int>{2, 4}));
}

TEST_CASE("core small vector - shrink to fit returns to inline storage or exact capacity",
          "[core][small_vector]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::small_vector<int, 2> values{&tracking};
  values.resize(6, 8);
  values.resize(4);

  values.shrink_to_fit();
  CHECK_FALSE(values.uses_inline_storage());
  CHECK(values.capacity() == 4);
  CHECK(std::ranges::equal(values, std::initializer_list<int>{8, 8, 8, 8}));

  values.resize(2);
  values.shrink_to_fit();
  CHECK(values.uses_inline_storage());
  CHECK(values.capacity() == 2);
  CHECK(std::ranges::equal(values, std::initializer_list<int>{8, 8}));
}

TEST_CASE("core small vector - initializer list and range construction preserve order",
          "[core][small_vector]") {
  caors::small_vector<int, 3> initialized{1, 2, 3, 4};
  const std::array source{5, 6, 7};
  caors::small_vector<int, 3> ranged{source.begin(), source.end()};

  CHECK(std::ranges::equal(initialized, std::initializer_list<int>{1, 2, 3, 4}));
  CHECK(std::ranges::equal(ranged, source));
}

TEST_CASE("core small vector - insert supports values ranges and self ranges",
          "[core][small_vector]") {
  caors::small_vector<int, 3> values{1, 4};
  auto inserted = values.insert(values.begin() + 1, 2);
  CHECK(inserted == values.begin() + 1);
  inserted = values.insert(values.begin() + 2, 1, 3);
  CHECK(inserted == values.begin() + 2);
  const std::array tail{5, 6};
  inserted = values.insert(values.end(), tail.begin(), tail.end());
  CHECK(inserted == values.begin() + 4);
  CHECK(std::ranges::equal(values, std::initializer_list<int>{1, 2, 3, 4, 5, 6}));

  const auto original_size = values.size();
  inserted = values.insert(values.end(), values.begin(), values.begin() + 2);
  CHECK(inserted == values.begin() + original_size);
  CHECK(std::ranges::equal(values, std::initializer_list<int>{1, 2, 3, 4, 5, 6, 1, 2}));
}

TEST_CASE("core small vector - erase returns the following element", "[core][small_vector]") {
  caors::small_vector<int, 2> values{1, 2, 3, 4, 5};

  auto next = values.erase(values.begin() + 1);
  REQUIRE(next != values.end());
  CHECK(*next == 3);
  next = values.erase(values.begin() + 1, values.begin() + 3);

  REQUIRE(next != values.end());
  CHECK(*next == 5);
  CHECK(std::ranges::equal(values, std::initializer_list<int>{1, 5}));
}

TEST_CASE("core small vector - insert and erase support move-only elements",
          "[core][small_vector]") {
  caors::small_vector<std::unique_ptr<int>, 1> values;
  values.emplace_back(std::make_unique<int>(2));
  values.emplace(values.begin(), std::make_unique<int>(1));

  REQUIRE(values.size() == 2);
  CHECK(*values[0] == 1);
  CHECK(*values[1] == 2);
  values.erase(values.begin());
  REQUIRE(values.size() == 1);
  CHECK(*values.front() == 2);
}

TEST_CASE("core small vector - assign stages aliased ranges before replacement",
          "[core][small_vector]") {
  caors::small_vector<int, 2> values{1, 2, 3, 4};
  values.assign(values.begin() + 1, values.end());
  CHECK(std::ranges::equal(values, std::initializer_list<int>{2, 3, 4}));

  values.assign(3, 9);
  CHECK(std::ranges::equal(values, std::initializer_list<int>{9, 9, 9}));
  values.assign({7, 8});
  CHECK(std::ranges::equal(values, std::initializer_list<int>{7, 8}));
}

TEST_CASE("core small vector - swap exchanges inline heap values and resources",
          "[core][small_vector]") {
  caors::tracking_resource first_resource{&caors::get_system_memory_resource()};
  caors::tracking_resource second_resource{&caors::get_system_memory_resource()};
  caors::small_vector<int, 2> first{{1, 2, 3}, &first_resource};
  caors::small_vector<int, 2> second{{8}, &second_resource};

  using std::swap;
  swap(first, second);

  CHECK(first.resource() == &second_resource);
  CHECK(second.resource() == &first_resource);
  CHECK(std::ranges::equal(first, std::initializer_list<int>{8}));
  CHECK(std::ranges::equal(second, std::initializer_list<int>{1, 2, 3}));
  first.swap(first);
  CHECK(std::ranges::equal(first, std::initializer_list<int>{8}));
}

TEST_CASE("core small vector - comparisons ignore capacity and resource", "[core][small_vector]") {
  caors::tracking_resource first_resource{&caors::get_system_memory_resource()};
  caors::tracking_resource second_resource{&caors::get_system_memory_resource()};
  caors::small_vector<int, 2> first{{1, 2}, &first_resource};
  caors::small_vector<int, 2> equal{{1, 2}, &second_resource};
  caors::small_vector<int, 2> greater{{1, 3}, &second_resource};
  equal.reserve(8);

  CHECK(first == equal);
  CHECK(first < greater);
}

#if CAORS_HAS_EXCEPTIONS
TEST_CASE("core small vector - at reports an empty range", "[core][small_vector]") {
  const caors::core::small_vector<int, 2> values;
  CHECK_THROWS_AS(values.at(0), std::out_of_range);
}

TEST_CASE("core small vector - failed relocation keeps the original inline state",
          "[core][small_vector]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::small_vector<throwing_copy_value, 2> values{&tracking};
  values.emplace_back(3);
  values.emplace_back(5);

  throwing_copy_value::throw_on_copy = true;
  CHECK_THROWS_AS(values.reserve(8), std::runtime_error);
  throwing_copy_value::throw_on_copy = false;

  REQUIRE(values.size() == 2);
  CHECK(values.capacity() == 2);
  CHECK(values.uses_inline_storage());
  CHECK(values[0].value == 3);
  CHECK(values[1].value == 5);
  const auto statistics = tracking.statistics();
  CHECK(statistics.allocation_count == 1);
  CHECK(statistics.deallocation_count == 1);
  CHECK(statistics.bytes_in_use == 0);
}

TEST_CASE("core small vector - failed copy assignment preserves the target",
          "[core][small_vector]") {
  caors::small_vector<throwing_copy_value, 2> source;
  source.emplace_back(3);
  caors::small_vector<throwing_copy_value, 2> target;
  target.emplace_back(9);

  throwing_copy_value::throw_on_copy = true;
  CHECK_THROWS_AS(target = source, std::runtime_error);
  throwing_copy_value::throw_on_copy = false;

  REQUIRE(target.size() == 1);
  CHECK(target.front().value == 9);
  REQUIRE(source.size() == 1);
  CHECK(source.front().value == 3);
}

TEST_CASE("core small vector - failed insertion keeps valid lifetime state",
          "[core][small_vector]") {
  throwing_move_assignment_value::alive = 0;
  {
    caors::small_vector<throwing_move_assignment_value, 4> values;
    values.emplace_back(1);
    values.emplace_back(2);
    values.emplace_back(3);

    throwing_move_assignment_value::throw_on_move_assignment = true;
    CHECK_THROWS_AS(values.emplace(values.begin() + 1, 9), std::runtime_error);
    throwing_move_assignment_value::throw_on_move_assignment = false;

    CHECK(values.size() == 3);
    CHECK(throwing_move_assignment_value::alive == 3);
  }
  CHECK(throwing_move_assignment_value::alive == 0);
}
#endif
