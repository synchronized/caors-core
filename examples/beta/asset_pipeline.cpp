// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "assets_example_common.h"

#include "caors/tasks.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace {

[[nodiscard]] bool drain_until_done(
    caors::tasks::execution_context& completions,
    const caors::resources::load_request<caors::examples::beta::texture_resource>& load,
    const caors::resources::preload_request& preload) {
  for (int frame = 0; frame < 200 && (!load.done() || !preload.done()); ++frame) {
    (void)completions.drain();
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
  }

  (void)completions.drain();
  return load.done() && preload.done();
}

[[nodiscard]] bool drain_until_done(
    caors::tasks::execution_context& completions,
    const caors::resources::load_request<caors::examples::beta::texture_resource>& request) {
  for (int frame = 0; frame < 200 && !request.done(); ++frame) {
    (void)completions.drain();
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
  }

  (void)completions.drain();
  return request.done();
}

[[nodiscard]] std::string bytes_to_string(const caors::io::byte_buffer& bytes) {
  return std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

} // namespace

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
  caors::tasks::execution_context completions;

  caors::resources::load_request<texture_resource> direct =
      assets.request_load<texture_resource>(texture_guid, executor, completions.get_executor());
  caors::resources::preload_request preload =
      assets.request_preload(texture_guid, executor, completions.get_executor());

  if (!drain_until_done(completions, direct, preload) || direct.failed() || preload.failed()) {
    std::cerr << "async load/preload pipeline failed\n";
    return 1;
  }

  caors::resource_ref<texture_resource> texture_ref{texture_guid};
  if (!assets.resolve(texture_ref) || texture_ref.get() == nullptr) {
    std::cerr << "texture ref resolve failed\n";
    return 1;
  }

  std::cout << "loaded texture: " << texture_ref.get()->width << "x" << texture_ref.get()->height
            << '\n';

  if (!write_texture(assets, 1024, 512)) {
    return 1;
  }

  caors::resources::load_request<texture_resource> reload =
      assets.request_reload<texture_resource>(texture_guid, executor, completions.get_executor());
  if (!drain_until_done(completions, reload) || reload.failed() || !reload.try_get()) {
    std::cerr << "async reload failed\n";
    return 1;
  }

  if (!assets.resolve(texture_ref)) {
    std::cerr << "texture ref resolve after reload failed\n";
    return 1;
  }

  texture_ref.get()->width = 2048;
  texture_ref.get()->height = 1024;
  caors::assets::json::saver<texture_resource> saver{assets.provider()};
  if (!check(saver.save(*texture_ref.get()), "save reloaded texture")) {
    return 1;
  }

  caors::io::memory_file_system* files = memory_files(assets);
  if (files == nullptr) {
    std::cerr << "memory file system was not created\n";
    return 1;
  }

  caors::io::byte_buffer_result saved = files->read_all("textures/hero.texture.json");
  if (saved.failed()) {
    std::cerr << "read saved texture failed\n";
    return 1;
  }

  std::cout << "saved texture json: " << bytes_to_string(saved.value) << '\n';
  return 0;
}
