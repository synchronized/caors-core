// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "assets_example_common.h"

#include <iostream>
#include <string_view>

namespace {

struct debug_source_state {
  int calls = 0;
};

struct debug_source_options {
  std::string mount;
  int priority = 0;
  std::string filter;
};

[[nodiscard]] bool register_debug_source_options() {
  static bool registered = false;
  if (registered) {
    return true;
  }

  const caors::error status =
      caors::registration::type_<debug_source_options>("example.beta.debug_source_options")
          .meta()
          .property("mount", &debug_source_options::mount, caors::meta::registration::required(),
                    caors::meta::registration::non_empty())
          .property("priority", &debug_source_options::priority,
                    caors::meta::registration::range(1, 10),
                    caors::meta::registration::default_value(1))
          .property("filter", &debug_source_options::filter,
                    caors::meta::registration::one_of(
                        {std::string_view{"nearest"}, std::string_view{"linear"}}),
                    caors::meta::registration::default_value("nearest"))
          .serialization()
          .commit();
  registered = status.succeeded();
  return caors::examples::beta::check(status, "register debug source options");
}

[[nodiscard]] caors::error
make_debug_source(caors::assets::asset_manager::source_factory_context& context,
                  const caors::assets::project_source_config& config,
                  const debug_source_options& options, void* user_data) {
  auto& state = *static_cast<debug_source_state*>(user_data);
  ++state.calls;
  (void)options;

  auto* file_system = context.manager.create_owned<caors::io::memory_file_system>(context.resource);
  if (file_system == nullptr) {
    return {caors::error_domain::resources, caors::errc::out_of_memory};
  }

  auto* source = context.manager.create_owned<caors::resources::file_source>(*file_system);
  if (source == nullptr) {
    return {caors::error_domain::resources, caors::errc::out_of_memory};
  }

  return context.manager.bind_source(config.name, *source);
}

} // namespace

int main() {
  if (!register_debug_source_options()) {
    return 1;
  }

  caors::assets::project_config config;
  if (!caors::examples::beta::check(caors::assets::json::load_project_config(config,
                                                                             R"({
                "sources": [
                  {
                    "name": "assets",
                    "type": "debug_source",
                    "options": {
                      "mount": "",
                      "priority": 99,
                      "filter": "cubic"
                    }
                  }
                ]
              })"),
                                    "load config")) {
    return 1;
  }

  debug_source_state state;
  caors::assets::asset_manager assets;
  if (!caors::examples::beta::check(
          assets.register_typed_source_factory<debug_source_options, &make_debug_source>(
              caors::string_name{"debug_source"}, &state),
          "register source factory")) {
    return 1;
  }

  const caors::assets::asset_build_result result = assets.build_detailed(config);
  std::cout << "schema validation status: " << static_cast<int>(result.status.value()) << '\n';
  std::cout << "factory calls: " << state.calls << '\n';
  for (const caors::string& reason : result.reasons) {
    std::cout << "reason: " << reason << '\n';
  }

  return result.failed() && state.calls == 0 ? 0 : 1;
}
