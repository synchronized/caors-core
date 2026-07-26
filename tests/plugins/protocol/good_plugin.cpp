// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/abi.h"
#include "caors/plugins/meta_registration.h"
#include "caors/registration.h"

#include <cstring>

namespace {

struct plugin_reflected_type {
  int value = 42;
};

struct plugin_instance {
  int lifecycle_state = 0;
  caors_plugin_host host{};
};

plugin_instance instance{};
constexpr int extension_value = 42;

int32_t CAORS_PLUGIN_CALL initialize(void* data) {
  auto& self = *static_cast<plugin_instance*>(data);
  if (self.lifecycle_state != 0) {
    return 1;
  }
  if (caors::meta::registry* registry =
          caors::plugins::find_registration_registry(&self.host, data);
      registry != nullptr) {
    caors::meta::context context{*registry};
    if (caors::registration::type_<plugin_reflected_type>(context, "caors.test.plugin.reflected")
            .meta()
            .property("value", &plugin_reflected_type::value)
            .commit()) {
      return 1;
    }
  }
  self.lifecycle_state = 1;
  return 0;
}

int32_t CAORS_PLUGIN_CALL start(void* data) {
  auto& self = *static_cast<plugin_instance*>(data);
  if (self.lifecycle_state != 1) {
    return 1;
  }
  self.lifecycle_state = 2;
  return 0;
}

int32_t CAORS_PLUGIN_CALL stop(void* data) {
  auto& self = *static_cast<plugin_instance*>(data);
  if (self.lifecycle_state != 2) {
    return 1;
  }
  self.lifecycle_state = 3;
  return 0;
}

int32_t CAORS_PLUGIN_CALL shutdown(void* data) {
  auto& self = *static_cast<plugin_instance*>(data);
  if (self.lifecycle_state != 1 && self.lifecycle_state != 3) {
    return 1;
  }
  self.lifecycle_state = 0;
  return 0;
}

const void* CAORS_PLUGIN_CALL get_extension(void*, const char* id, size_t id_size,
                                            uint32_t version) {
  constexpr char extension_id[] = "caors.test.answer";
  return version == 1 && id_size == sizeof(extension_id) - 1 &&
                 std::memcmp(id, extension_id, id_size) == 0
             ? &extension_value
             : nullptr;
}

constexpr char plugin_name[] = "caors.test.good";
constexpr char plugin_display_name[] = "CAORS 测试插件";

const caors_plugin_descriptor descriptor{
    sizeof(caors_plugin_descriptor),
    {{0x2a, 0x6d, 0x59, 0x92, 0xff, 0x3c, 0x45, 0xae, 0xa1, 0x67, 0xd8, 0x61, 0xe4, 0x69, 0xaa,
      0x90}},
    {plugin_name, sizeof(plugin_name) - 1},
    {plugin_display_name, sizeof(plugin_display_name) - 1},
    {1, 2, 3},
};

const caors_plugin_entry entry{
    sizeof(caors_plugin_entry),
    CAORS_PLUGIN_ABI_VERSION,
    &descriptor,
    &instance,
    &initialize,
    &start,
    &stop,
    &shutdown,
    &get_extension,
};

} // namespace

extern "C" CAORS_PLUGIN_EXPORT const caors_plugin_entry* CAORS_PLUGIN_CALL
caors_query_plugin(const caors_plugin_host* host, uint32_t host_abi_version) {
  if (host == nullptr || host->struct_size < sizeof(caors_plugin_host) ||
      host_abi_version != CAORS_PLUGIN_ABI_VERSION || host->get_extension == nullptr) {
    return nullptr;
  }
  instance.host = *host;
  const auto* runtime = static_cast<const caors_plugin_runtime_extension*>(host->get_extension(
      host->context, CAORS_PLUGIN_RUNTIME_EXTENSION_ID,
      sizeof(CAORS_PLUGIN_RUNTIME_EXTENSION_ID) - 1, CAORS_PLUGIN_RUNTIME_EXTENSION_VERSION));
  if (runtime == nullptr || runtime->struct_size < sizeof(caors_plugin_runtime_extension) ||
      host->get_extension(host->context, CAORS_PLUGIN_RUNTIME_EXTENSION_ID,
                          sizeof(CAORS_PLUGIN_RUNTIME_EXTENSION_ID) - 1, 2) != nullptr) {
    return nullptr;
  }
  if (host->log != nullptr) {
    constexpr char message[] = "插件已协商宿主扩展";
    host->log(host->context, 1, message, sizeof(message) - 1);
  }
  return &entry;
}
