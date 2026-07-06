// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - generated ids are valid and unique", "[object]") {
  const caors::object_id generated = caors::objects::generate_object_id();
  runtime_actor first;
  runtime_actor second;

  CHECK(static_cast<bool>(generated));
  CHECK(static_cast<bool>(first.id()));
  CHECK(static_cast<bool>(second.id()));
  CHECK(generated != first.id());
  CHECK(generated != second.id());
  CHECK(first.id() != second.id());
}

TEST_CASE("object - generated ids are unique across threads", "[object]") {
  constexpr std::size_t thread_count = 4;
  constexpr std::size_t ids_per_thread = 256;

  std::vector<std::vector<caors::object_id>> generated(thread_count);
  std::vector<std::thread> threads;
  threads.reserve(thread_count);

  for (std::size_t thread_index = 0; thread_index < thread_count; ++thread_index) {
    generated[thread_index].reserve(ids_per_thread);
    threads.emplace_back([&generated, thread_index] {
      for (std::size_t index = 0; index < ids_per_thread; ++index) {
        generated[thread_index].push_back(caors::objects::generate_object_id());
      }
    });
  }

  for (std::thread& thread : threads) {
    thread.join();
  }

  std::unordered_set<std::uint64_t> unique_ids;
  unique_ids.reserve(thread_count * ids_per_thread);
  for (const std::vector<caors::object_id>& ids : generated) {
    for (caors::object_id id : ids) {
      REQUIRE(id);
      CHECK(unique_ids.insert(id.value()).second);
    }
  }

  CHECK(unique_ids.size() == thread_count * ids_per_thread);
}

TEST_CASE("object - object_id supports standard unordered containers", "[object]") {
  const caors::object_id id = caors::objects::generate_object_id();
  REQUIRE(id);

  std::unordered_map<caors::object_id, int> values;
  values.emplace(id, 7);

  CHECK(values.at(id) == 7);
}
