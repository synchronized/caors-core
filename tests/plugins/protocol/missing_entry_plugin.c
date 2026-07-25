// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/abi.h"

CAORS_PLUGIN_EXPORT int CAORS_PLUGIN_CALL caors_not_a_plugin_entry(void) {
  caors_plugin_descriptor descriptor = {0};
  return (int)descriptor.struct_size;
}
