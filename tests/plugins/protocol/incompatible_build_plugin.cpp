// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/abi.h"

#include <cstring>

namespace {

const caors_plugin_build_extension build{
    sizeof(caors_plugin_build_extension),
    CAORS_PLUGIN_BUILD_EXTENSION_VERSION,
    {CAORS_PLUGIN_CURRENT_COMPILER, CAORS_PLUGIN_CURRENT_COMPILER_VERSION,
     CAORS_PLUGIN_CURRENT_ABI_VERSION,
     CAORS_PLUGIN_CURRENT_CRT_MODE == CAORS_PLUGIN_CRT_DYNAMIC_RELEASE
         ? CAORS_PLUGIN_CRT_DYNAMIC_DEBUG
         : CAORS_PLUGIN_CRT_DYNAMIC_RELEASE,
     CAORS_PLUGIN_CURRENT_ITERATOR_DEBUG_LEVEL, static_cast<uint32_t>(sizeof(void*)),
     CAORS_PLUGIN_CURRENT_ARCHITECTURE}};

const void* CAORS_PLUGIN_CALL get_extension(void*, const char* id, size_t id_size,
                                            uint32_t version) {
  return version == CAORS_PLUGIN_BUILD_EXTENSION_VERSION &&
                 id_size == sizeof(CAORS_PLUGIN_BUILD_EXTENSION_ID) - 1 &&
                 std::memcmp(id, CAORS_PLUGIN_BUILD_EXTENSION_ID, id_size) == 0
             ? &build
             : nullptr;
}

constexpr char plugin_name[] = "caors.test.incompatible-build";
const caors_plugin_descriptor descriptor{
    sizeof(caors_plugin_descriptor), {{3}}, {plugin_name, sizeof(plugin_name) - 1}, {}, {1, 0, 0}};
const caors_plugin_entry entry{sizeof(caors_plugin_entry),
                               CAORS_PLUGIN_ABI_VERSION,
                               &descriptor,
                               nullptr,
                               nullptr,
                               nullptr,
                               nullptr,
                               nullptr,
                               &get_extension};

} // namespace

extern "C" CAORS_PLUGIN_EXPORT const caors_plugin_entry* CAORS_PLUGIN_CALL
caors_query_plugin(const caors_plugin_host*, uint32_t) {
  return &entry;
}
