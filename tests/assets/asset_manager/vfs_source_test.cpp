// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "../../resources/resources_test_common.h"
using namespace caors::tests::resources;

TEST_CASE("assets - new VFS source captures routes for default JSON loader",
          "[assets][vfs-context]") {
  require_serialized_resource_metadata();
  caors::tasks::execution_context worker, completion;
  caors::io::vfs routes;
  auto old = caors::io::make_memory_backend();
  auto next = caors::io::make_memory_backend();
  REQUIRE_FALSE(old->store("data", R"({"width":111})"));
  REQUIRE_FALSE(next->store("data", R"({"width":222})"));
  REQUIRE_FALSE(routes.mount("assets", old));
  caors::resources::vfs_source source{routes};
  caors::assets::asset_manager assets{{worker.get_executor(), completion.get_executor()}};
  caors::assets::project_builder project{assets};
  const caors::string_name type{"new.vfs.json"};
  REQUIRE_FALSE(caors::assets::json::register_type_factory<serialized_resource>(project, type));
  caors::assets::project_config config;
  config.resource_types.push_back(
      {type, caors::string_name{"json"}, caors::string_name{"memory"}, {}});
  REQUIRE_FALSE(project.build(config));
  REQUIRE_FALSE(project.provider().bind(test_guid_a, source, "assets/data"));
  auto request = assets.request_load<serialized_resource>(test_guid_a);
  REQUIRE(routes.unmount("assets"));
  REQUIRE_FALSE(routes.mount("assets", next));
  for (int i = 0; i < 32 && !request.done(); ++i) {
    (void)worker.drain();
    (void)completion.drain();
  }
  REQUIRE(request.loaded());
  CHECK(request.try_get()->width == 111);
  auto reload = assets.request_reload<serialized_resource>(test_guid_a);
  for (int i = 0; i < 32 && !reload.done(); ++i) {
    (void)worker.drain();
    (void)completion.drain();
  }
  REQUIRE(reload.loaded());
  CHECK(reload.try_get()->width == 222);
}
