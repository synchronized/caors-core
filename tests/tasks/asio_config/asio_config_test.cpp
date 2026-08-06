// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include <asio/io_context.hpp>
#include <asio/post.hpp>

#include <catch2/catch_all.hpp>

TEST_CASE("tasks - standalone Asio configuration can dispatch work", "[tasks][asio]") {
  asio::io_context context;
  int observed = 0;

  asio::post(context, [&observed] { observed = 42; });

  CHECK(context.poll() == 1);
  CHECK(observed == 42);
}
