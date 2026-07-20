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

  caors::resources::handle_result<texture_resource> first =
      assets.load_handle<texture_resource>(texture_guid);
  if (first.failed() || !first.value) {
    std::cerr << "initial load failed\n";
    return 1;
  }

  if (!write_texture(assets, 1024, 512)) {
    return 1;
  }

  caors::resources::handle_result<texture_resource> reloaded =
      assets.reload_handle<texture_resource>(texture_guid);
  if (reloaded.failed() || !reloaded.value) {
    std::cerr << "reload texture failed\n";
    return 1;
  }

  std::cout << "old handle still sees: " << first.value->width << "x" << first.value->height
            << '\n';
  std::cout << "cache now sees: " << reloaded.value->width << "x" << reloaded.value->height << '\n';
  return 0;
}
