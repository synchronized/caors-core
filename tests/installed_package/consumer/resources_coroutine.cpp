// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/assets.h"
#include "caors/io.h"
#include "caors/resources.h"
#include "caors/resources/coroutine.h"
#include "caors/tasks/execution_context.h"
#include "caors/tasks/inline_executor.h"
#include <optional>

#include <coroutine>
#include <iostream>
#include <utility>

namespace {

constexpr caors::resource_guid consumer_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x35, 0x40, 0x3d, 0xf4, 0x88, 0xe2, 0x4e, 0xe2, 0xa8, 0x93, 0x75, 0x13,
                            0x8b, 0x25, 0x3b, 0x19})};

class consumer_resource final : public caors::resource {
public:
  using caors::resource::resource;

  int value = 0;
};

class consumer_loader final : public caors::resources::loader<consumer_resource> {
public:
  [[nodiscard]] caors::resources::load_result<consumer_resource>
  load(caors::resource_guid guid, caors::memory_resource* resource = nullptr) override {
    auto loaded = caors::memory::make_unique<consumer_resource>(resource, guid);
    loaded->value = 42;
    return {{}, std::move(loaded)};
  }
};

class native_loader final : public caors::resources::async_loader<consumer_resource> {
public:
  caors::error start(caors::resources::async_load_context value,
                     caors::resources::load_completion<consumer_resource> token) override {
    context.emplace(std::move(value));
    completion = std::move(token);
    return {};
  }
  void finish() {
    auto token = std::move(completion);
    auto value = caors::memory::make_unique<consumer_resource>(context->resource, context->guid);
    value->value = -1;
    auto sources = context->snapshot_as<caors::resources::provider_snapshot>();
    auto file = sources ? sources->resolve(context->guid)
                        : caors::fail<caors::io::resolved_file>(
                              {caors::error_domain::io, caors::errc::unsupported});
    if (file) {
      auto opened = caors::io::request_open(std::move(file.value), caors::tasks::inline_executor());
      if (opened && opened->result() && *opened->result()) {
        auto stream = opened->result()->value;
        auto read = caors::io::request_read(stream, 0, 1, 1, caors::tasks::inline_executor());
        if (read && read->result() && *read->result())
          value->value = std::to_integer<int>(read->result()->value[0]);
        auto closed = caors::io::request_close(stream, caors::tasks::inline_executor());
        if (!closed || !closed->result() || !*closed->result())
          value->value = -2;
      }
    }
    context.reset();
    (void)token.complete({{}, std::move(value)});
  }
  std::optional<caors::resources::async_load_context> context;
  caors::resources::load_completion<consumer_resource> completion;
};

class probe_coroutine final {
public:
  struct promise_type {
    [[nodiscard]] probe_coroutine get_return_object() noexcept {
      return probe_coroutine{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    [[nodiscard]] std::suspend_never initial_suspend() const noexcept { return {}; }
    [[nodiscard]] std::suspend_always final_suspend() const noexcept { return {}; }
    void return_void() const noexcept {}
    void unhandled_exception() noexcept { failed = true; }

    bool failed = false;
  };

  explicit probe_coroutine(std::coroutine_handle<promise_type> handle) noexcept : handle_(handle) {}
  probe_coroutine(const probe_coroutine&) = delete;
  probe_coroutine& operator=(const probe_coroutine&) = delete;
  probe_coroutine(probe_coroutine&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
  ~probe_coroutine() {
    if (handle_) {
      handle_.destroy();
    }
  }

  [[nodiscard]] bool done() const noexcept { return handle_.done(); }
  [[nodiscard]] bool failed() const noexcept { return handle_.promise().failed; }

private:
  std::coroutine_handle<promise_type> handle_{};
};

probe_coroutine wait_for_resource(caors::resources::load_request<consumer_resource> request,
                                  caors::resources::handle_result<consumer_resource>* result) {
  *result = co_await caors::resources::async_wait(std::move(request));
}

} // namespace

int main() {
  caors::resource_cache<consumer_resource> cache;
  consumer_loader loader;
  caors::resource_database database;
  if (caors::error status = database.register_type(cache, loader)) {
    std::cerr << "CAORS installed resources coroutine consumer 注册失败\n";
    return 1;
  }

  caors::tasks::execution_context completion_context;
  auto request = database.request_load<consumer_resource>(
      consumer_guid, caors::tasks::inline_executor(), completion_context.get_executor());
  caors::resources::handle_result<consumer_resource> result;
  probe_coroutine operation = wait_for_resource(request, &result);

  if (operation.done() || completion_context.poll() != 1 || !operation.done() ||
      operation.failed() || result.failed() || !result.value || result.value->value != 42 ||
      cache.find(consumer_guid) != result.value.get()) {
    std::cerr << "CAORS installed resources coroutine consumer 失败\n";
    return 1;
  }

  native_loader native;
  caors::assets::asset_manager assets{
      {caors::tasks::inline_executor(), completion_context.get_executor()}};
  caors::assets::project_builder project{assets};
  if (project.build_detailed(caors::assets::project_config{}).failed())
    return 7;
  auto* owned = project.create_owned<int>(42);
  if (!owned || *owned != 42)
    return 8;
  if (assets.register_type(cache, native, caors::core::string_name{"consumer.native"})) {
    return 2;
  }
  auto files = std::make_shared<caors::io::memory_file_system>();
  const std::byte payload[]{std::byte{64}};
  if (files->write_all("data", payload))
    return 5;
  const caors::string_name backend_name{"consumer.memory"};
  if (project.bind_file_system(backend_name, files) ||
      project.find_owned_file_system(backend_name) != files)
    return 9;
  caors::resources::file_source source{files};
  if (assets.provider().bind(consumer_guid, source, "data"))
    return 6;
  auto group =
      assets.create_group(caors::tasks::inline_executor(), completion_context.get_executor());
  auto reload = group.request_refresh<consumer_resource>(consumer_guid);
  caors::resources::handle_result<consumer_resource> native_result;
  probe_coroutine native_operation = wait_for_resource(reload, &native_result);
  if (native_operation.done() || !native.context) {
    return 3;
  }
  assets.provider().clear();
  files.reset();
  native.finish();
  if (completion_context.drain() != 1 || !native_operation.done() || native_operation.failed() ||
      native_result.failed() || native_result.value->value != 64 ||
      assets.database().has_active_operations()) {
    return 4;
  }

  std::cout << "CAORS installed resources coroutine consumer passed\n";
  return 0;
}
