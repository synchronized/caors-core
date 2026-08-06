// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/resources/coroutine.h"

#include <coroutine>
#include <utility>

namespace {

class probe_resource final : public caors::resource {
public:
  using caors::resource::resource;
};

class probe_operation final {
public:
  struct promise_type {
    [[nodiscard]] probe_operation get_return_object() noexcept { return {}; }
    [[nodiscard]] std::suspend_never initial_suspend() const noexcept { return {}; }
    [[nodiscard]] std::suspend_never final_suspend() const noexcept { return {}; }
    void return_value(caors::resources::handle_result<probe_resource>) const noexcept {}
    void unhandled_exception() const noexcept {}
  };
};

// 该探针会额外使用 `-fno-exceptions` 独立编译，确保资源 awaiter 不依赖异常语法。
[[maybe_unused]] probe_operation
no_exceptions_load_probe(caors::resources::load_request<probe_resource> request) {
  co_return co_await caors::resources::async_wait(std::move(request));
}

} // namespace
