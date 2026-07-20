// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "assets_example_common.h"

#include <iostream>
#include <string_view>

namespace {

struct debug_source_state {
  int calls = 0;
};

[[nodiscard]] caors::error
make_debug_source(caors::assets::asset_manager::source_factory_context& context,
                  const caors::assets::project_source_config& config, void* user_data) {
  auto& state = *static_cast<debug_source_state*>(user_data);
  ++state.calls;

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

[[nodiscard]] caors::assets::factory_option_schema make_source_schema() {
  caors::assets::factory_option_schema schema;
  schema.require(caors::string_name{"mount"}, caors::assets::factory_option_kind::string)
      .non_empty(caors::string_name{"mount"})
      .optional(caors::string_name{"priority"})
      .range(caors::string_name{"priority"}, 1.0, 10.0)
      .optional(caors::string_name{"filter"}, caors::assets::factory_option_kind::string)
      .one_of(caors::string_name{"filter"},
              {std::string_view{"nearest"}, std::string_view{"linear"}});
  schema.allow_unknown = false;
  return schema;
}

} // namespace

int main() {
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
                      "filter": "cubic",
                      "extra": true
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
          assets.register_source_factory(caors::string_name{"debug_source"}, &make_debug_source,
                                         &state, make_source_schema()),
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
