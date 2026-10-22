// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_EXAMPLES_BETA_ASSETS_EXAMPLE_COMMON_H_
#define CAORS_EXAMPLES_BETA_ASSETS_EXAMPLE_COMMON_H_

#include "caors/assets.h"
#include "caors/assets/json.h"
#include "caors/io.h"
#include "caors/registration.h"
#include "caors/resources.h"

#include <iostream>
#include <string>
#include <string_view>

namespace caors::examples::beta {

constexpr resource_guid texture_guid{
    guid::from_bytes(guid::byte_array{0x54, 0x4c, 0x93, 0x86, 0x6d, 0x2f, 0x47, 0xb4, 0xa0, 0x9a,
                                      0x34, 0x20, 0x54, 0x13, 0xf6, 0x91})};

[[nodiscard]] inline string_name texture_manifest_type() {
  return string_name{"example.beta.texture"};
}

class texture_resource : public resource {
public:
  using resource::resource;

  int width = 0;
  int height = 0;
};

[[nodiscard]] inline bool check(error status, std::string_view step) {
  if (!status) {
    return true;
  }

  std::cerr << "failed at " << step << ": " << static_cast<int>(status.value()) << '\n';
  return false;
}

[[nodiscard]] inline bool register_texture_type() {
  static bool registered = false;
  if (registered) {
    return true;
  }

  const error status = registration::type_<texture_resource>("example.beta.texture_resource")
                           .meta()
                           .property("width", &texture_resource::width)
                           .property("height", &texture_resource::height)
                           .serialization()
                           .commit();
  registered = status.succeeded();
  return check(status, "register texture_resource");
}

[[nodiscard]] inline std::string make_project_config_json() {
  std::string json{R"({
    "file_systems": [
      {
        "name": "memory",
        "type": "memory_file_system"
      },
      {
        "name": "vfs",
        "type": "resolver",
        "routes": [
          {
            "prefix": "caors://assets",
            "target": "memory"
          }
        ]
      }
    ],
    "sources": [
      {
        "name": "assets",
        "file_system": "vfs"
      }
    ],
    "resource_types": [
      {
        "type": "example.beta.texture",
        "loader": "json",
        "cache": "memory"
      }
    ],
    "resources": [
      {
        "guid": ")"};
  json += texture_guid.string();
  json += R"(",
        "source": "assets",
        "location": "caors://assets/textures/hero.texture.json",
        "type": "example.beta.texture"
      }
    ],
    "preload": [
      ")";
  json += texture_guid.string();
  json += R"("
    ]
  })";
  return json;
}

[[nodiscard]] inline io::memory_file_system* memory_files(assets::asset_manager& manager) {
  return dynamic_cast<io::memory_file_system*>(
      caors::assets::project_builder{manager}.find_file_system(string_name{"memory"}));
}

[[nodiscard]] inline bool configure_assets(assets::asset_manager& manager) {
  assets::project_config config;
  if (!check(assets::json::load_project_config(config, make_project_config_json()),
             "load project config")) {
    return false;
  }

  return check(assets::json::register_type_factory<texture_resource>(
                   caors::assets::project_builder{manager}, texture_manifest_type()),
               "register texture factory") &&
         check(caors::assets::project_builder{manager}.build(config), "build asset manager");
}

[[nodiscard]] inline bool write_texture(assets::asset_manager& manager, int width, int height) {
  io::memory_file_system* files = memory_files(manager);
  if (files == nullptr) {
    std::cerr << "memory file system was not created\n";
    return false;
  }

  std::string text{"{\"width\":"};
  text += std::to_string(width);
  text += R"(,"height":)";
  text += std::to_string(height);
  text += "}";
  return check(files->write_all("textures/hero.texture.json", text), "write texture");
}

} // namespace caors::examples::beta

#endif
