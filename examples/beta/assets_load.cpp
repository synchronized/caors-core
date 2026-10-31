// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "assets_example_common.h"

#include <iostream>

int main() {
  using namespace caors::examples::beta;

  if (!register_texture_type()) {
    return 1;
  }

  caors::tasks::execution_context worker;
  caors::tasks::execution_context completion;
  caors::assets::asset_manager assets{{worker.get_executor(), completion.get_executor()}};
  if (!configure_assets(assets) || !write_texture(assets, 256, 128)) {
    return 1;
  }

  auto request = assets.request_load<texture_resource>(texture_guid);
  for (int frame = 0; frame < 32 && !request.done(); ++frame) {
    (void)worker.drain();
    (void)completion.drain();
  }
  const auto loaded = request.result();
  if (loaded.failed() || loaded.value == nullptr) {
    std::cerr << "load texture failed\n";
    return 1;
  }

  std::cout << "loaded texture: " << loaded.value->width << "x" << loaded.value->height << '\n';
  return 0;
}
