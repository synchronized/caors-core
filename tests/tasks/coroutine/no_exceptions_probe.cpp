// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks/coroutine.h"

#include <utility>

namespace {

// 该探针会额外使用 `-fno-exceptions` 独立编译，确保 coroutine 公共 API 不依赖异常语法。
[[maybe_unused]] caors::tasks::async_operation no_exceptions_probe(caors::tasks::executor target,
                                                                   caors::tasks::request source) {
  if (caors::error status = co_await caors::tasks::schedule(std::move(target))) {
    co_return status;
  }
  co_return co_await caors::tasks::async_wait(std::move(source));
}

} // namespace
