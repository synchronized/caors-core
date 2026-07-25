// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/abi.h"

#include <cstring>

namespace {

int lifecycle_state = 0;
constexpr int extension_value = 42;

int32_t CAORS_PLUGIN_CALL initialize(void*) {
  if (lifecycle_state != 0) {
    return 1;
  }
  lifecycle_state = 1;
  return 0;
}

int32_t CAORS_PLUGIN_CALL start(void*) {
  if (lifecycle_state != 1) {
    return 1;
  }
  lifecycle_state = 2;
  return 0;
}

int32_t CAORS_PLUGIN_CALL stop(void*) {
  if (lifecycle_state != 2) {
    return 1;
  }
  lifecycle_state = 3;
  return 0;
}

int32_t CAORS_PLUGIN_CALL shutdown(void*) {
  if (lifecycle_state != 1 && lifecycle_state != 3) {
    return 1;
  }
  lifecycle_state = 0;
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
    nullptr,
    &initialize,
    &start,
    &stop,
    &shutdown,
    &get_extension,
};

} // namespace

extern "C" CAORS_PLUGIN_EXPORT const caors_plugin_entry* CAORS_PLUGIN_CALL
caors_query_plugin(const caors_plugin_host* host, uint32_t host_abi_version) {
  return host != nullptr && host->struct_size >= sizeof(caors_plugin_host) &&
                 host_abi_version == CAORS_PLUGIN_ABI_VERSION
             ? &entry
             : nullptr;
}
