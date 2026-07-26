// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TESTS_PLUGINS_PROTOCOL_TEST_EXTENSION_H_
#define CAORS_TESTS_PLUGINS_PROTOCOL_TEST_EXTENSION_H_

#include "caors/plugins/abi.h"

#include <stddef.h>
#include <stdint.h>

#define CAORS_PLUGIN_PROTOCOL_TEST_EXTENSION_ID "caors.test.protocol-control"
#define CAORS_PLUGIN_PROTOCOL_TEST_EXTENSION_VERSION 1u

typedef void(CAORS_PLUGIN_CALL* caors_plugin_test_control_fn)(void* context);
typedef int32_t(CAORS_PLUGIN_CALL* caors_plugin_test_query_fn)(void* context);

typedef struct caors_plugin_protocol_test_extension {
  uint32_t struct_size;
  uint32_t version;
  void* context;
  caors_plugin_test_control_fn reset;
  caors_plugin_test_query_fn entered;
  caors_plugin_test_control_fn release;
  caors_plugin_test_query_fn destructions;
} caors_plugin_protocol_test_extension;

typedef struct caors_plugin_protocol_capability {
  int32_t value;
} caors_plugin_protocol_capability;

typedef struct caors_plugin_protocol_meta_extension {
  int32_t value;
} caors_plugin_protocol_meta_extension;

#ifdef __cplusplus

#include "caors/meta/proxy.h"

CAORS_META_DEF_MEM_DISPATCH(caors_plugin_protocol_proxy_value, proxy_value);

using caors_plugin_protocol_proxy_capability =
    caors::meta::capability_builder::add_convention<caors_plugin_protocol_proxy_value,
                                                    int() noexcept>::build;

#endif

#endif
