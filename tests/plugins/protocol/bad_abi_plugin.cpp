// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/abi.h"

namespace {

constexpr char plugin_name[] = "caors.test.bad_abi";
const caors_plugin_descriptor descriptor{
    sizeof(caors_plugin_descriptor), {{1}}, {plugin_name, sizeof(plugin_name) - 1}, {}, {1, 0, 0},
};
const caors_plugin_entry entry{
    sizeof(caors_plugin_entry),
    CAORS_PLUGIN_ABI_VERSION + 1,
    &descriptor,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

} // namespace

extern "C" CAORS_PLUGIN_EXPORT const caors_plugin_entry* CAORS_PLUGIN_CALL
caors_query_plugin(const caors_plugin_host*, uint32_t) {
  return &entry;
}
