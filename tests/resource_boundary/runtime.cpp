// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/resources/resource_manager.h"
#include <iostream>
#include <string>
#include <vector>

namespace {
struct host_image {
  explicit host_image(int value) : value(value) {}
  int value;
};
class host_loader final : public caors::resources::loader<host_image, std::string> {
public:
  caors::error start(caors::resources::basic_load_context<std::string> context,
                     caors::resources::load_completion<host_image> completion) override {
    keys.push_back(std::move(context.key));
    pending.push_back(std::move(completion));
    return {};
  }
  bool finish(std::size_t index, int value) {
    return pending[index].complete({{}, caors::memory::make_unique<host_image>(nullptr, value)});
  }
  std::vector<std::string> keys;
  std::vector<caors::resources::load_completion<host_image>> pending;
};
}

int main() {
  caors::resources::cache<host_image, std::string> cache;
  caors::resources::load_request_table<host_image, std::string> requests;
  host_loader loader;
  caors::tasks::execution_context worker, completion;
  auto first = caors::resources::request_load_into(
      cache, loader, "network/image", worker.get_executor(), completion.get_executor(), requests);
  auto second = caors::resources::request_load_into(
      cache, loader, "network/image", worker.get_executor(), completion.get_executor(), requests);
  (void)worker.drain();
  if (loader.keys.size() != 1 || first.done() || !first.cancel() || !loader.finish(0, 42))
    return 1;
  (void)completion.drain();
  if (!second.result() || second.result().value->value != 42 || !cache.contains("network/image"))
    return 2;
  auto old = second.result().value;
  auto reload = caors::resources::request_reload_into(
      cache, loader, "network/image", worker.get_executor(), completion.get_executor(), requests);
  (void)worker.drain();
  if (loader.keys.size() != 2 || !loader.finish(1, 43))
    return 3;
  (void)completion.drain();
  if (!reload.result() || cache.find("network/image")->value != 43 || old->value != 42)
    return 4;
  caors::resources::ref<host_image, std::string> reference{std::string{"network/image"}};
  if (!cache.resolve(reference) || !cache.is_current(reference))
    return 5;
  caors::resources::load_request<host_image> late;
  {
    caors::resources::resource_manager<std::string> manager{
        {worker.get_executor(), completion.get_executor()}};
    if (manager.register_loader(loader))
      return 6;
    late = manager.request_load<host_image>("late");
    (void)worker.drain();
  }
  if (!loader.finish(2, 99))
    return 7;
  (void)completion.drain();
  if (!late.result() || late.result().value->value != 99)
    return 8;
  std::cout << "host path loading: shared request, cancellation, cache, reload, reference and "
               "manager lifetime passed\n";
}
