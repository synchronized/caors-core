// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/assets.h"
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
    case 3:
      value.loader.finish();
      value.completion.clear();
      check(value.request.failed() && !value.cache->contains(guid));
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
