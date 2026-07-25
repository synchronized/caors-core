// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/abi.h"

namespace {

int32_t CAORS_PLUGIN_CALL fail_initialize(void*) { return 1; }
int32_t CAORS_PLUGIN_CALL succeed(void*) { return 0; }

constexpr char plugin_name[] = "caors.test.failure";
const caors_plugin_descriptor descriptor{
    sizeof(caors_plugin_descriptor), {{2}}, {plugin_name, sizeof(plugin_name) - 1}, {}, {1, 0, 0},
};
const caors_plugin_entry entry{
    sizeof(caors_plugin_entry),
    CAORS_PLUGIN_ABI_VERSION,
    &descriptor,
    nullptr,
    &fail_initialize,
    &succeed,
    &succeed,
    &succeed,
    nullptr,
};

} // namespace

extern "C" CAORS_PLUGIN_EXPORT const caors_plugin_entry* CAORS_PLUGIN_CALL
caors_query_plugin(const caors_plugin_host*, uint32_t) {
  return &entry;
}
