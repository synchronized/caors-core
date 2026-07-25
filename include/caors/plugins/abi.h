// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_PLUGINS_ABI_H_
#define CAORS_PLUGINS_ABI_H_

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#define CAORS_PLUGIN_EXPORT __declspec(dllexport)
#define CAORS_PLUGIN_CALL __cdecl
#elif defined(__GNUC__) || defined(__clang__)
#define CAORS_PLUGIN_EXPORT __attribute__((visibility("default")))
#define CAORS_PLUGIN_CALL
#else
#define CAORS_PLUGIN_EXPORT
#define CAORS_PLUGIN_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define CAORS_PLUGIN_ABI_VERSION 1u
#define CAORS_PLUGIN_ENTRY_SYMBOL "caors_query_plugin"

typedef struct caors_plugin_string_view {
  const char* data;
  size_t size;
} caors_plugin_string_view;

typedef struct caors_plugin_guid {
  uint8_t bytes[16];
} caors_plugin_guid;

typedef struct caors_plugin_version {
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
} caors_plugin_version;

typedef struct caors_plugin_descriptor {
  uint32_t struct_size;
  caors_plugin_guid id;
  caors_plugin_string_view name;
  caors_plugin_string_view display_name;
  caors_plugin_version version;
} caors_plugin_descriptor;

typedef void(CAORS_PLUGIN_CALL* caors_plugin_log_fn)(void* context, uint32_t level,
                                                     const char* message, size_t message_size);
typedef const void*(CAORS_PLUGIN_CALL* caors_plugin_get_extension_fn)(void* context, const char* id,
                                                                      size_t id_size,
                                                                      uint32_t version);

typedef struct caors_plugin_host {
  uint32_t struct_size;
  uint32_t abi_version;
  void* context;
  caors_plugin_log_fn log;
  caors_plugin_get_extension_fn get_extension;
} caors_plugin_host;

typedef int32_t(CAORS_PLUGIN_CALL* caors_plugin_lifecycle_fn)(void* plugin_data);

typedef struct caors_plugin_entry {
  uint32_t struct_size;
  uint32_t abi_version;
  const caors_plugin_descriptor* descriptor;
  void* plugin_data;
  caors_plugin_lifecycle_fn initialize;
  caors_plugin_lifecycle_fn start;
  caors_plugin_lifecycle_fn stop;
  caors_plugin_lifecycle_fn shutdown;
  caors_plugin_get_extension_fn get_extension;
} caors_plugin_entry;

typedef const caors_plugin_entry*(CAORS_PLUGIN_CALL* caors_query_plugin_fn)(
    const caors_plugin_host* host, uint32_t host_abi_version);

#ifdef __cplusplus
}
#endif

#endif
