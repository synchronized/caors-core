// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "assets_example_common.h"

#include <iostream>

int main() {
  using namespace caors::examples::beta;

  if (!register_texture_type()) {
    return 1;
  }

  caors::assets::asset_manager assets;
  if (!configure_assets(assets) || !write_texture(assets, 256, 128)) {
    return 1;
  }

  const caors::result<texture_resource*> loaded = assets.load<texture_resource>(texture_guid);
  if (loaded.failed() || loaded.value == nullptr) {
    std::cerr << "load texture failed\n";
    return 1;
  }

  std::cout << "sync texture: " << loaded.value->width << "x" << loaded.value->height << '\n';
  return 0;
}
