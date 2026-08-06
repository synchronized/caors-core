// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "assets_example_common.h"

#include "caors/tasks.h"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
  using namespace caors::examples::beta;

  if (!register_texture_type()) {
    return 1;
  }

  caors::assets::asset_manager assets;
  if (!configure_assets(assets) || !write_texture(assets, 512, 256)) {
    return 1;
  }

  caors::tasks::thread_pool pool{1};
  caors::tasks::executor executor = pool.get_executor();
  caors::tasks::completion_queue completions;
  caors::resources::load_request<texture_resource> request =
      assets.request_load<texture_resource>(texture_guid, executor, completions.get_executor());

  for (int frame = 0; frame < 200 && !request.done(); ++frame) {
    (void)completions.drain();
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
  }
  (void)completions.drain();

  const caors::resources::handle_result<texture_resource> loaded = request.result();
  if (loaded.failed() || !loaded.value) {
    std::cerr << "async load texture failed\n";
    return 1;
  }

  std::cout << "async texture: " << loaded.value->width << "x" << loaded.value->height
            << ", progress=" << request.progress() << '\n';
  return 0;
}
