// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/assets.h"
#include "caors/assets/json.h"
#include "caors/io/context.h"
#include "caors/io/memory_file_system.h"
#include "caors/registration.h"
#include "caors/resources.h"
#include "caors/tasks.h"
#include <emscripten.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {
class texture final : public caors::resource {
public:
  using resource::resource;
};
class delayed_loader final : public caors::resources::loader<texture> {
public:
  struct operation {
    caors::resources::async_load_context context;
    caors::resources::load_completion<texture> completion;
  };
  std::optional<operation> pending;
  caors::error start(caors::resources::async_load_context context,
                     caors::resources::load_completion<texture> completion) override {
    pending.emplace(operation{std::move(context), std::move(completion)});
    return {};
  }
  void finish() {
    auto current = std::move(*pending);
    pending.reset();
    if (!current.completion.complete(
            {{},
             caors::memory::make_unique<texture>(current.context.resource, current.context.guid)}))
      throw std::runtime_error("completion rejected");
  }
};
const caors::resource_guid guid{caors::guid::from_bytes(caors::guid::byte_array{1, 2, 3})};
void check(bool condition) {
  if (!condition)
    throw std::runtime_error("browser protocol assertion");
}
struct state {
  caors::tasks::execution_context worker;
  caors::tasks::execution_context completion;
  delayed_loader loader;
  std::unique_ptr<caors::resources::cache<texture>> cache;
  std::unique_ptr<caors::assets::asset_manager> assets;
  caors::resources::load_request<texture> request;
  caors::resources::handle<texture> kept;
  caors::resources::file_byte_reader byte_reader;
  caors::io::io_request<caors::io::byte_buffer> bytes;
  caors::io::request<caors::io::byte_buffer> context_read;
  int phase = 0;
  void begin() {
    cache = std::make_unique<caors::resources::cache<texture>>();
    assets = std::make_unique<caors::assets::asset_manager>(
        caors::assets::asset_execution_options{worker.get_executor(), completion.get_executor()});
    check(!assets->register_type(*cache, loader, caors::string_name{"web.texture"}));
    request = assets->request_load<texture>(guid);
    check(worker.drain() == 1);
    check(loader.pending.has_value() && !request.done());
  }
};
std::unique_ptr<state> current;
void tick(void*) {
  try {
    auto& value = *current;
    switch (value.phase++) {
    case 0:
      value.loader.finish();
      check(!value.request.done());
      break;
    case 1:
      check(value.completion.drain() == 1 && value.request.loaded());
      value.kept = value.request.try_get();
      value.request = {};
      value.assets.reset();
      value.cache.reset();
      check(value.kept && value.kept->guid() == guid);
      value.begin();
      check(value.request.cancel());
      break;
    case 2:
      value.loader.finish();
      static_cast<void>(value.completion.drain());
      check(value.request.cancelled() && !value.cache->contains(guid));
      value.assets.reset();
      value.cache.reset();
      value.begin();
      break;
    case 3: {
      value.loader.finish();
      value.completion.clear();
      check(value.request.failed() && !value.cache->contains(guid));
      auto files = std::make_shared<caors::io::memory_file_system>();
      check(!files->write_all("data", "abc"));
      auto reading = caors::resources::request_bytes(
          value.byte_reader,
          {{files, caors::io::path{"data"}, 0}, value.worker.get_executor(), 3, nullptr},
          value.completion.get_executor());
      check(static_cast<bool>(reading));
      value.bytes = std::move(reading.value);
      break;
    }
    case 4:
      (void)value.worker.drain();
      (void)value.completion.drain();
      if (!value.bytes.result()) {
        --value.phase;
        break;
      }
      check(static_cast<bool>(*value.bytes.result()));
      check(value.bytes.result()->value.size() == 3);
      value.request = {};
      value.assets.reset();
      value.cache.reset();
      check(
          !caors::registration::type_<texture>("web.json.texture").meta().serialization().commit());
      value.assets =
          std::make_unique<caors::assets::asset_manager>(caors::assets::asset_execution_options{
              value.worker.get_executor(), value.completion.get_executor()});
      {
        caors::assets::project_builder project{*value.assets};
        const caors::string_name type{"web.json.texture"};
        check(!caors::assets::json::register_type_factory<texture>(project, type));
        caors::assets::project_config config;
        config.resource_types.push_back(
            {type, caors::string_name{"json"}, caors::string_name{"memory"}, {}});
        check(!project.build(config));
        auto files = std::make_shared<caors::io::memory_file_system>();
        check(!files->write_all("texture", "{}"));
        caors::resources::file_source source{files};
        check(!project.provider().bind(guid, source, "texture"));
        value.request = value.assets->request_load<texture>(guid);
      }
      break;
    case 5:
      (void)value.worker.drain();
      (void)value.completion.drain();
      if (!value.request.done()) {
        --value.phase;
        break;
      }
      check(value.request.loaded());
      check(!value.assets->database().has_active_operations());
      {
        caors::io::vfs routes;
        auto memory = caors::io::make_memory_backend();
        check(!memory->store("data", "browser"));
        check(!routes.mount("assets", memory));
        caors::io::io_context io{routes, {value.completion.get_executor()}};
        value.context_read = io.read("assets/data");
      }
      break;
    case 6:
      (void)value.completion.drain();
      if (!value.context_read.done()) {
        --value.phase;
        break;
      }
      check(static_cast<bool>(*value.context_read.result()));
      check(value.context_read.result()->value.size() == 7);
      current.reset();
      EM_ASM({
        document.body.dataset.caorsStatus = "passed";
        document.body.textContent = "CAORS browser protocol passed";
      });
      return;
    }
    // 刻意跨越浏览器事件循环，禁止用阻塞轮询伪造异步完成。
    emscripten_async_call(&tick, nullptr, 10);
  } catch (...) {
    current.reset();
    EM_ASM({ document.body.dataset.caorsStatus = "failed"; });
  }
}
} // namespace
int main() {
  current = std::make_unique<state>();
  current->begin();
  emscripten_async_call(&tick, nullptr, 10);
}
