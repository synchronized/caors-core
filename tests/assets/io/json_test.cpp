// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "../../resources/resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - json loader loads resource through provider", "[resources]") {
  require_serialized_resource_metadata();

  const std::filesystem::path directory = make_temp_dir();
  const std::filesystem::path file = directory / "resource.json";
  std::error_code ec;
  std::filesystem::remove_all(directory, ec);
  REQUIRE(std::filesystem::create_directories(directory, ec));
  REQUIRE_FALSE(ec);

  caors::io::native_file_system native;
  const std::string file_path = path_string(file);
  REQUIRE(native.write_all(file_path, std::string_view{R"({"width":512})"}).succeeded());

  caors::resources::file_source source{native};
  caors::resources::provider provider;
  REQUIRE(provider.bind(test_guid_a, source, file_path).succeeded());

  caors::assets::json::sync_loader<serialized_resource> loader{provider};
  caors::resource_cache<serialized_resource> cache;

  caors::resources::handle_result<serialized_resource> loaded =
      caors::resources::load_handle_into(cache, loader, test_guid_a);

  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value);
  CHECK(loaded.value->guid() == test_guid_a);
  CHECK(loaded.value->width == 512);
  CHECK(cache.find(test_guid_a) == loaded.value.get());

  std::filesystem::remove_all(directory, ec);
}

TEST_CASE("resources - json loader result survives scratch rewind", "[resources][memory]") {
  require_serialized_resource_metadata();

  caors::io::memory_file_system files;
  REQUIRE(files.write_all("assets/resource.json", R"({"width":768})").succeeded());
  caors::resources::file_source source{files};
  caors::resources::provider provider;
  REQUIRE(provider.bind(test_guid_a, source, "assets/resource.json").succeeded());

  caors::tracking_resource result_tracking{&caors::get_system_memory_resource()};
  caors::tracking_resource scratch_tracking{&caors::get_system_memory_resource()};
  caors::stack_resource scratch{64, &scratch_tracking};
  caors::serialization::context context{};
  context.resolvers.memory.result_resource = &result_tracking;
  context.resolvers.memory.scratch_resource = &scratch;

  caors::assets::json::sync_loader<serialized_resource> loader{context, provider};
  caors::resources::load_result<serialized_resource> loaded =
      loader.load(test_guid_a, &result_tracking);
  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);
  CHECK(scratch.active_scope_count() == 0);

  scratch.reset();
  CHECK(scratch_tracking.statistics().bytes_in_use == 0);
  CHECK(loaded.value->guid() == test_guid_a);
  CHECK(loaded.value->width == 768);
  CHECK(result_tracking.statistics().bytes_in_use > 0);
}

#include "../../resources/resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - json loader reports parse failures", "[resources]") {
  const std::filesystem::path directory = make_temp_dir();
  const std::filesystem::path file = directory / "invalid.json";
  std::error_code ec;
  std::filesystem::remove_all(directory, ec);
  REQUIRE(std::filesystem::create_directories(directory, ec));
  REQUIRE_FALSE(ec);

  caors::io::native_file_system native;
  const std::string file_path = path_string(file);
  REQUIRE(native.write_all(file_path, std::string_view{R"({"width":)"}).succeeded());

  caors::resources::file_source source{native};
  caors::resources::provider provider;
  REQUIRE(provider.bind(test_guid_a, source, file_path).succeeded());

  caors::assets::json::sync_loader<serialized_resource> loader{provider};
  caors::resources::load_result<serialized_resource> loaded = loader.load(test_guid_a);

  REQUIRE(loaded.failed());
  CHECK(loaded.status.domain() == caors::error_domain::serialization);
  CHECK(loaded.status.value() == caors::errc::invalid_argument);

  std::filesystem::remove_all(directory, ec);
}

#include "../../resources/resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - json saver reports invalid and missing targets", "[resources]") {
  require_serialized_resource_metadata();

  caors::io::memory_file_system files;
  caors::resources::file_source source{files};
  caors::resources::provider provider;
  REQUIRE(provider.bind(test_guid_a, source, "assets/resource.json").succeeded());

  caors::assets::json::saver<serialized_resource> saver{provider};

  serialized_resource invalid_resource{};
  caors::error invalid = saver.save(invalid_resource);
  REQUIRE(invalid.failed());
  CHECK(invalid.domain() == caors::error_domain::resources);
  CHECK(invalid.value() == caors::errc::invalid_argument);

  serialized_resource missing_resource{test_guid_b};
  missing_resource.width = 7;
  caors::error missing = saver.save(missing_resource);
  REQUIRE(missing.failed());
  CHECK(missing.domain() == caors::error_domain::resources);
  CHECK(missing.value() == caors::errc::not_found);
}

#include "../../resources/resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - json saver saves resource through provider", "[resources]") {
  require_serialized_resource_metadata();

  caors::io::memory_file_system files;
  caors::resources::file_source source{files};
  caors::resources::provider provider;
  REQUIRE(provider.bind(test_guid_a, source, "assets/resource.json").succeeded());

  serialized_resource value{test_guid_a};
  value.width = 640;

  caors::assets::json::saver<serialized_resource> saver{provider};
  REQUIRE(saver.save(value).succeeded());

  caors::io::byte_buffer_result bytes = files.read_all("assets/resource.json");
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        R"({"width":640})");

  caors::assets::json::sync_loader<serialized_resource> loader{provider};
  caors::resource_cache<serialized_resource> cache;
  caors::resources::handle_result<serialized_resource> loaded =
      caors::resources::load_handle_into(cache, loader, test_guid_a);

  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value);
  CHECK(loaded.value->guid() == test_guid_a);
  CHECK(loaded.value->width == 640);
}

TEST_CASE("assets - project config can load through format agnostic loader", "[assets]") {
  caors::io::memory_file_system files;
  REQUIRE(files
              .write_all("project.caors", std::string_view{R"({
    "schema_version": 1,
    "sources": [
      {
        "name": "assets",
        "type": "memory_file_system",
        "root": "assets"
      }
    ],
    "resource_types": [
      {
        "type": "caors.test.resources.serialized_resource",
        "loader": "json",
        "cache": "memory"
      }
    ],
    "resources": [
      {
        "guid": "6cd2da3d-b549-4756-8b8b-0c84293af410",
        "source": "assets",
        "location": "resource.json",
        "type": "caors.test.resources.serialized_resource",
        "hash": "hash",
        "version": 2
      }
    ],
    "dependencies": [
      {
        "resource": "0c2f82f8-2b3e-4f77-965c-4f2d1c4a907a",
        "dependency": "6cd2da3d-b549-4756-8b8b-0c84293af410"
      }
    ],
    "preload": [
      "0c2f82f8-2b3e-4f77-965c-4f2d1c4a907a"
    ]
  })"})
              .succeeded());

  caors::resources::file_source source{files};
  caors::assets::project_config config;
  caors::assets::json::project_config_loader loader;

  REQUIRE(caors::assets::load_project_config(config, loader, source, "project.caors").succeeded());

  REQUIRE(config.sources.size() == 1);
  CHECK(config.sources[0].name == caors::string_name{"assets"});
  CHECK(config.sources[0].type == caors::string_name{"memory_file_system"});
  CHECK(config.sources[0].root.string_view() == "assets");

  REQUIRE(config.resource_types.size() == 1);
  CHECK(config.resource_types[0].type ==
        caors::string_name{"caors.test.resources.serialized_resource"});
  CHECK(config.resource_types[0].loader == caors::string_name{"json"});
  CHECK(config.resource_types[0].cache == caors::string_name{"memory"});

  REQUIRE(config.resources.size() == 1);
  CHECK(config.resources[0].guid == test_guid_a);
  CHECK(config.resources[0].source == caors::string_name{"assets"});
  CHECK(config.resources[0].location.string_view() == "resource.json");
  CHECK(config.resources[0].hash == "hash");
  CHECK(config.resources[0].version == 2);

  REQUIRE(config.dependencies.size() == 1);
  CHECK(config.dependencies[0].resource == test_guid_b);
  CHECK(config.dependencies[0].dependency == test_guid_a);

  REQUIRE(config.preload_roots.size() == 1);
  CHECK(config.preload_roots[0] == test_guid_b);
}

TEST_CASE("assets - project config can save and roundtrip file systems", "[assets]") {
  std::string json{R"({
    "schema_version": 1,
    "file_systems": [
      {
        "name": "memory",
        "type": "memory_file_system",
        "options": {
          "label": "debug"
        }
      },
      {
        "name": "mounted",
        "type": "mount",
        "routes": [
          {
            "prefix": "/assets",
            "target": "memory",
            "target_prefix": "pack"
          }
        ]
      },
      {
        "name": "vfs",
        "type": "resolver",
        "routes": [
          {
            "prefix": "caors://game",
            "target": "mounted"
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
        "type": "caors.test.resources.serialized_resource",
        "loader": "json",
        "cache": "memory"
      }
    ],
    "resources": [
      {
        "guid": "6cd2da3d-b549-4756-8b8b-0c84293af410",
        "source": "assets",
        "location": "caors://game//assets/resource.json",
        "type": "caors.test.resources.serialized_resource",
        "hash": "hash",
        "version": 7
      }
    ],
    "dependencies": [
      {
        "resource": "0c2f82f8-2b3e-4f77-965c-4f2d1c4a907a",
        "dependency": "6cd2da3d-b549-4756-8b8b-0c84293af410"
      }
    ],
    "preload": [
      "0c2f82f8-2b3e-4f77-965c-4f2d1c4a907a"
    ]
  })"};

  caors::assets::project_config config;
  REQUIRE(caors::assets::json::load_project_config(config, json).succeeded());

  const caors::serialization::serialization_text_result saved =
      caors::assets::json::save_project_config(config);
  REQUIRE(saved.succeeded());

  caors::assets::project_config roundtrip;
  REQUIRE(caors::assets::json::load_project_config(roundtrip, saved.value).succeeded());

  REQUIRE(roundtrip.file_systems.size() == 3);
  CHECK(roundtrip.file_systems[0].name == caors::string_name{"memory"});
  CHECK(roundtrip.file_systems[0].type == caors::string_name{"memory_file_system"});
  REQUIRE(roundtrip.file_systems[0].options.is_object());
  CHECK(roundtrip.file_systems[0]
            .options.find_member_variant(caors::string_name{"label"})
            ->as_string() == std::string_view{"debug"});
  REQUIRE(roundtrip.file_systems[1].routes.size() == 1);
  CHECK(roundtrip.file_systems[1].routes[0].prefix.string_view() == "assets");
  CHECK(roundtrip.file_systems[1].routes[0].target == caors::string_name{"memory"});
  CHECK(roundtrip.file_systems[1].routes[0].target_prefix.string_view() == "pack");
  CHECK(roundtrip.file_systems[2].routes[0].prefix.string_view() == "caors://game");

  REQUIRE(roundtrip.sources.size() == 1);
  CHECK(roundtrip.sources[0].type == caors::string_name{"file_system"});
  CHECK(roundtrip.sources[0].file_system == caors::string_name{"vfs"});

  REQUIRE(roundtrip.resources.size() == 1);
  CHECK(roundtrip.resources[0].location.string_view() ==
        std::string_view{"caors://game/assets/resource.json"});
  CHECK(roundtrip.resources[0].version == 7);

  caors::io::memory_file_system files;
  caors::resources::file_source source{files};
  REQUIRE(caors::assets::json::save_project_config_to(config, source, "project.caors").succeeded());

  caors::assets::project_config from_source;
  REQUIRE(
      caors::assets::json::load_project_config(from_source, source, "project.caors").succeeded());
  CHECK(from_source.file_systems.size() == roundtrip.file_systems.size());
  CHECK(from_source.resources[0].guid == test_guid_a);
}

TEST_CASE("assets - byte loader reads snapshot using native or worker reader",
          "[assets][byte-reader]") {
  require_serialized_resource_metadata();
  const auto mode = GENERATE(caors::resources::file_read_mode::native_async,
                             caors::resources::file_read_mode::worker_adapter);
  caors::tasks::execution_context worker;
  caors::tasks::execution_context completion;
  caors::resources::file_byte_reader reader{mode};
  caors::assets::json::loader<serialized_resource> loader{reader};
  caors::resources::cache<serialized_resource> cache;
  caors::assets::asset_manager assets{{worker.get_executor(), completion.get_executor()}};
  auto files = std::make_shared<caors::io::memory_file_system>();
  REQUIRE_FALSE(files->write_all("old", R"({"width":123})"));
  REQUIRE_FALSE(files->write_all("new", R"({"width":456})"));
  caors::resources::file_source source{files};
  auto& provider = caors::assets::project_builder{assets}.provider();
  REQUIRE_FALSE(provider.bind(test_guid_a, source, "old"));
  REQUIRE_FALSE(assets.register_type(cache, loader, caors::string_name{"byte.json"}));
  auto request = assets.request_load<serialized_resource>(test_guid_a);
  REQUIRE_FALSE(provider.rebind(test_guid_a, source, "new"));
  CHECK_FALSE(request.done());
  for (int i = 0; i < 32 && !request.done(); ++i) {
    (void)worker.drain();
    (void)completion.drain();
  }
  REQUIRE(request.loaded());
  CHECK(request.try_get()->width == 123);
  auto refreshed = assets.request_refresh<serialized_resource>(test_guid_a);
  for (int i = 0; i < 32 && !refreshed.done(); ++i) {
    (void)worker.drain();
    (void)completion.drain();
  }
  REQUIRE(refreshed.loaded());
  CHECK(refreshed.try_get()->width == 456);
  REQUIRE_FALSE(files->write_all("new", "{"));
  auto invalid = assets.request_refresh<serialized_resource>(test_guid_a);
  for (int i = 0; i < 32 && !invalid.done(); ++i) {
    (void)worker.drain();
    (void)completion.drain();
  }
  REQUIRE(invalid.failed());
  REQUIRE(cache.find(test_guid_a));
  CHECK(cache.find(test_guid_a)->width == 456);
  CHECK_FALSE(assets.database().has_active_operations());
}

TEST_CASE("assets - byte reader enforces budget and cancellation", "[assets][byte-reader]") {
  caors::tasks::execution_context worker;
  caors::tasks::execution_context completion;
  auto files = std::make_shared<caors::io::memory_file_system>();
  REQUIRE_FALSE(files->write_all("data", "oversized"));
  caors::resources::file_source source{files};
  caors::resources::provider provider;
  REQUIRE_FALSE(provider.bind(test_guid_a, source, "data"));
  auto snapshot = provider.snapshot();
  auto resolved = snapshot->resolve(test_guid_a);
  REQUIRE(resolved);
  caors::resources::file_byte_reader reader;
  int notifications = 0;
  auto request = caors::resources::request_bytes(
      reader, {resolved.value, worker.get_executor(), 2, nullptr}, completion.get_executor(),
      [&](const auto&) { ++notifications; });
  REQUIRE(request);
  for (int i = 0; i < 32 && !request.value.result(); ++i) {
    (void)worker.drain();
    (void)completion.drain();
  }
  REQUIRE(request.value.result());
  CHECK(request.value.result()->error().value == caors::io::io_errc::limit_exceeded);
  CHECK(notifications == 1);
  auto cancelled = caors::resources::request_bytes(
      reader, {resolved.value, worker.get_executor(), 32, nullptr}, completion.get_executor());
  REQUIRE(cancelled);
  cancelled.value.cancel();
  for (int i = 0; i < 32 && !cancelled.value.result(); ++i) {
    (void)worker.drain();
    (void)completion.drain();
  }
  REQUIRE(cancelled.value.result());
  CHECK(cancelled.value.result()->error().value == caors::io::io_errc::cancelled);
}
