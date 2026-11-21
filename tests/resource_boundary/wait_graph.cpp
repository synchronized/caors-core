// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/resources/detail/wait_graph.h"

#include <barrier>
#include <cstdlib>
#include <thread>

namespace {
using graph = caors::resources::detail::wait_graph;
using result = graph::link_result;
void require(bool condition) {
  if (!condition)
    std::abort();
}
} // namespace

int main() {
  graph values;
  const auto root = *values.insert();
  const auto left = *values.insert();
  const auto right = *values.insert();
  const auto shared = *values.insert();
  require(values.link(root, left) == result::linked);
  require(values.link(root, right) == result::linked);
  require(values.link(left, shared) == result::linked);
  require(values.link(right, shared) == result::linked);
  require(values.link(shared, root) == result::cycle);
  require(values.link(root, root) == result::cycle);
  require(values.link(left, shared) == result::linked);
  values.unlink(left, shared);
  require(values.link(shared, left) == result::cycle);
  values.unlink(left, shared);
  require(values.link(shared, left) == result::linked);
  values.erase(shared);
  values.unlink(left, shared);
  require(values.link(left, shared) == result::missing_node);
  const auto replacement = *values.insert();
  require(replacement != shared);
  require(values.link(left, replacement) == result::linked);

  graph limited{nullptr, 2, 0};
  auto first = *limited.insert();
  auto second = *limited.insert();
  require(!limited.insert());
  require(limited.link(first, second) == result::limit);
  limited.erase(first);
  require(limited.insert().has_value());

  for (int iteration = 0; iteration < 100; ++iteration) {
    graph concurrent;
    const auto a = *concurrent.insert();
    const auto b = *concurrent.insert();
    std::barrier start{3};
    result forward{}, backward{};
    std::thread one{[&] {
      start.arrive_and_wait();
      forward = concurrent.link(a, b);
    }};
    std::thread two{[&] {
      start.arrive_and_wait();
      backward = concurrent.link(b, a);
    }};
    start.arrive_and_wait();
    one.join();
    two.join();
    require((forward == result::linked && backward == result::cycle) ||
            (backward == result::linked && forward == result::cycle));
  }
}
