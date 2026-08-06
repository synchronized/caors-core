// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks/asio.h"
#include "caors/tasks/execution_context.h"
#include "caors/tasks/inline_executor.h"

#include <iostream>
#include <utility>

namespace {

caors::tasks::async_operation switch_once(caors::tasks::executor target, int* observed) {
  if (caors::error status = co_await caors::tasks::schedule(std::move(target))) {
    co_return status;
  }
  *observed = 9;
  co_return caors::error{};
}

} // namespace

int main() {
  caors::tasks::execution_context context;
  caors::tasks::inline_executor start;
  int observed = 0;

  caors::tasks::request request =
      caors::tasks::spawn(start, switch_once(context.get_executor(), &observed));
  if (request.status() != caors::tasks::request_status::running || context.poll() != 1 ||
      !request.completed() || request.result() || observed != 9) {
    std::cerr << "CAORS installed coroutine consumer 失败\n";
    return 1;
  }

  std::cout << "CAORS installed coroutine consumer passed\n";
  return 0;
}
