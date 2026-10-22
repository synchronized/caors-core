// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/assets.h"
#include "caors/assets/json.h"
#include "caors/io.h"
#include "caors/meta.h"
#include "caors/registration.h"
#include "caors/resources.h"
#include "caors/resources/serialization.h"
#include "caors/serialization.h"

#include <iostream>
#include <string>
#include <string_view>

namespace {

constexpr caors::resource_guid hero_texture_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x0e, 0xa1, 0x90, 0x5c, 0x4c, 0x83, 0x49, 0x8f, 0x9b, 0x77, 0x0f, 0x1b,
                            0xf3, 0x42, 0x91, 0x10})};

constexpr caors::resource_guid hero_material_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x2d, 0xf7, 0x3d, 0x92, 0x2b, 0x58, 0x40, 0xa0, 0xb2, 0x86, 0x7f, 0x3c,
                            0x2b, 0x2f, 0x31, 0x20})};

[[nodiscard]] caors::string_name texture_manifest_type() {
  return caors::string_name{"example.texture"};
}

[[nodiscard]] caors::string_name material_manifest_type() {
  return caors::string_name{"example.material"};
}

class texture_resource : public caors::resource {
public:
  using caors::resource::resource;

  int width = 0;
  int height = 0;
};

class material_resource : public caors::resource {
public:
  using caors::resource::resource;

  caors::string name{};
  caors::resource_ref<texture_resource> diffuse{};
};

struct actor_config {
  caors::string name{};
  caors::resource_ref<material_resource> material{};
};

[[nodiscard]] bool check(caors::error status, std::string_view step) {
  if (!status) {
    return true;
  }

  std::cerr << "failed at " << step << ": " << static_cast<int>(status.value()) << '\n';
  return false;
}

[[nodiscard]] bool register_types() {
  if (!check(caors::registration::type_<texture_resource>("example.alpha.texture_resource")
                 .meta()
                 .property("width", &texture_resource::width)
                 .property("height", &texture_resource::height)
                 .serialization()
                 .commit(),
             "register texture_resource")) {
    return false;
  }

  if (!check(caors::registration::type_<caors::resource_ref<texture_resource>>(
                 "caors.resource_ref<example.alpha.texture_resource>")
                 .commit(),
             "register texture resource_ref")) {
    return false;
  }

  if (!check(caors::registration::type_<material_resource>("example.alpha.material_resource")
                 .meta()
                 .property("name", &material_resource::name)
                 .property("diffuse", &material_resource::diffuse)
                 .serialization()
                 .commit(),
             "register material_resource")) {
    return false;
  }

  if (!check(caors::registration::type_<caors::resource_ref<material_resource>>(
                 "caors.resource_ref<example.alpha.material_resource>")
                 .commit(),
             "register material resource_ref")) {
    return false;
  }

  return check(caors::registration::type_<actor_config>("example.alpha.actor_config")
                   .meta()
                   .property("name", &actor_config::name)
                   .property("material", &actor_config::material)
                   .serialization()
                   .commit(),
               "register actor_config");
}

[[nodiscard]] std::string make_project_config_json() {
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
        "type": "example.texture",
        "loader": "json",
        "cache": "memory"
      },
      {
        "type": "example.material",
        "loader": "json",
        "cache": "memory"
      }
    ],
    "resources": [
      {
        "guid": ")"};
  json += hero_texture_guid.string();
  json += R"(",
        "source": "assets",
        "location": "caors://assets/textures/hero.texture.json",
        "type": "example.texture"
      },
      {
        "guid": ")";
  json += hero_material_guid.string();
  json += R"(",
        "source": "assets",
        "location": "caors://assets/materials/hero.material.json",
        "type": "example.material"
      }
    ],
    "dependencies": [
      {
        "resource": ")";
  json += hero_material_guid.string();
  json += R"(",
        "dependency": ")";
  json += hero_texture_guid.string();
  json += R"("
      }
    ],
    "preload": [
      ")";
  json += hero_material_guid.string();
  json += R"("
    ]
  })";
  return json;
}

[[nodiscard]] bool fill_memory_assets(caors::assets::asset_manager& assets) {
  auto* files = dynamic_cast<caors::io::memory_file_system*>(
      assets.find_file_system(caors::string_name{"memory"}));
  if (files == nullptr) {
    std::cerr << "memory file system was not created\n";
    return false;
  }

  if (!check(files->write_all("textures/hero.texture.json", R"({"width":256,"height":128})"),
             "write texture")) {
    return false;
  }

  std::string material{R"({"name":"hero","diffuse":")"};
  material += hero_texture_guid.string();
  material += R"("})";
  return check(files->write_all("materials/hero.material.json", material), "write material");
}

} // namespace

int main() {
  if (!register_types()) {
    return 1;
  }

  caors::assets::project_config config;
  if (!check(caors::assets::json::load_project_config(config, make_project_config_json()),
             "load project config")) {
    return 1;
  }

  caors::assets::asset_manager assets;
  if (!check(caors::assets::json::register_type_factory<texture_resource>(assets,
                                                                          texture_manifest_type()),
             "register texture factory") ||
      !check(caors::assets::json::register_type_factory<material_resource>(
                 assets, material_manifest_type()),
             "register material factory") ||
      !check(caors::assets::project_builder{assets}.build(config), "build asset manager") ||
      !fill_memory_assets(assets)) {
    return 1;
  }

  caors::resources::preload_result preloaded = assets.preload(hero_material_guid);
  if (!check(preloaded.status, "preload material dependency closure")) {
    return 1;
  }

  material_resource* material = assets.find<material_resource>(hero_material_guid);
  if (material == nullptr || !assets.resolve(material->diffuse) ||
      material->diffuse.get() == nullptr) {
    std::cerr << "material dependency was not resolved\n";
    return 1;
  }

  const caors::resources::resource_ref_resolver_ops resolver =
      caors::assets::make_resource_resolver(assets);
  caors::serialization::context resolve_context{};
  resolve_context.resolvers.external_refs.resolver = &resolver;
  resolve_context.resolvers.external_refs.resolution =
      caors::resources::resource_ref_resolution::require_resolved;

  actor_config actor{};
  std::string actor_json{R"({"name":"hero","material":")"};
  actor_json += hero_material_guid.string();
  actor_json += R"("})";
  if (!check(caors::read_json_from(resolve_context, actor, actor_json),
             "deserialize actor config")) {
    return 1;
  }

  caors::resources::resource_dependency_list dependencies;
  const caors::resources::resource_dependency_collector_ops collector =
      caors::resources::make_resource_dependency_collector(dependencies);
  caors::serialization::context collect_context{};
  collect_context.resolvers.external_refs.collector = &collector;

  caors::serialization::serialization_text_result saved = caors::to_json(collect_context, actor);
  if (saved.failed()) {
    std::cerr << "failed to serialize actor\n";
    return 1;
  }

  auto* files = static_cast<caors::io::memory_file_system*>(
      assets.find_file_system(caors::string_name{"memory"}));
  if (!check(files->write_all("scenes/hero.actor.json", saved.value), "save actor config")) {
    return 1;
  }

  const texture_resource* diffuse = material->diffuse.get();
  std::cout << "preloaded resources: " << preloaded.loaded.size() << '\n';
  std::cout << "actor: " << actor.name << '\n';
  std::cout << "material: " << actor.material->name << '\n';
  std::cout << "texture: " << diffuse->width << "x" << diffuse->height << '\n';
  std::cout << "collected dependencies: " << dependencies.size() << '\n';
  std::cout << "saved actor: " << saved.value << '\n';

  return 0;
}
