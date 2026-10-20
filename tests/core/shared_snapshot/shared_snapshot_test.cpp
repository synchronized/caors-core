// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/config.h"
#include "caors/core/detail/shared_snapshot.h"
#include <atomic>
#include <catch2/catch_all.hpp>
#include <memory>
#include <thread>
#include <vector>

TEST_CASE("快照回退保持 CAS 地址和共享所有权语义", "[snapshot]") {
  int object = 7;
  auto first = std::shared_ptr<int>(&object, [](int*) {});
  auto other_owner = std::shared_ptr<int>(&object, [](int*) {});
  caors::detail::shared_snapshot<int, false> slot{first};
  auto desired = std::make_shared<int>(9);
  REQUIRE_FALSE(slot.compare_exchange_strong(other_owner, desired));
  REQUIRE_FALSE(other_owner.owner_before(first));
  REQUIRE_FALSE(first.owner_before(other_owner));
  REQUIRE(slot.compare_exchange_strong(other_owner, desired));
  REQUIRE(*slot.load() == 9);
  REQUIRE(*first == 7);

  auto array = std::make_shared<std::pair<int, int>>(1, 2);
  auto left = std::shared_ptr<int>(array, &array->first);
  auto right = std::shared_ptr<int>(array, &array->second);
  slot.store(left);
  REQUIRE_FALSE(slot.compare_exchange_strong(right, desired));
  REQUIRE(right.get() == left.get());
}

TEST_CASE("快照回退在锁外销毁旧对象和失败 expected", "[snapshot]") {
  caors::detail::shared_snapshot<int, false> slot;
  bool destroyed = false;
  slot.store(std::shared_ptr<int>(new int(1), [&](int* value) {
    static_cast<void>(slot.load());
    destroyed = true;
    delete value;
  }));
  slot.store(std::make_shared<int>(2));
  REQUIRE(destroyed);
  destroyed = false;
  auto expected = std::shared_ptr<int>(new int(3), [&](int* value) {
    static_cast<void>(slot.load());
    destroyed = true;
    delete value;
  });
  REQUIRE_FALSE(slot.compare_exchange_strong(expected, {}));
  REQUIRE(destroyed);
  REQUIRE(*expected == 2);
}

#if CAORS_HAS_BACKGROUND_THREADS
TEST_CASE("快照回退并发 CAS 不丢失更新且旧快照保活", "[snapshot]") {
  caors::detail::shared_snapshot<const int, false> slot{std::make_shared<const int>(0)};
  auto original = slot.load();
  std::vector<std::thread> writers;
  for (int index = 0; index != 4; ++index) {
    writers.emplace_back([&] {
      for (int update = 0; update != 100; ++update) {
        auto expected = slot.load();
        while (
            !slot.compare_exchange_strong(expected, std::make_shared<const int>(*expected + 1))) {
        }
      }
    });
  }
  for (auto& writer : writers)
    writer.join();
  REQUIRE(*slot.load() == 400);
  REQUIRE(*original == 0);
}
#endif
