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
#define CAORS_PLUGIN_RUNTIME_EXTENSION_ID "caors.host.runtime"
#define CAORS_PLUGIN_RUNTIME_EXTENSION_VERSION 1u
#define CAORS_PLUGIN_SERVICES_EXTENSION_ID "caors.host.services"
#define CAORS_PLUGIN_SERVICES_EXTENSION_VERSION 1u

typedef enum caors_plugin_status {
  CAORS_PLUGIN_STATUS_SUCCESS = 0,
  CAORS_PLUGIN_STATUS_INVALID_ARGUMENT = 1,
  CAORS_PLUGIN_STATUS_ALREADY_EXISTS = 2,
  CAORS_PLUGIN_STATUS_OUT_OF_MEMORY = 3
} caors_plugin_status;

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

typedef struct caors_plugin_runtime_extension {
  uint32_t struct_size;
  uint32_t version;
  uint32_t runtime_major;
  uint32_t runtime_minor;
  uint32_t runtime_patch;
} caors_plugin_runtime_extension;

typedef int32_t(CAORS_PLUGIN_CALL* caors_plugin_publish_service_fn)(
    void* context, const caors_plugin_guid* id, uint32_t version, const void* service, void* owner);
typedef const void*(CAORS_PLUGIN_CALL* caors_plugin_find_service_fn)(void* context,
                                                                     const caors_plugin_guid* id,
                                                                     uint32_t minimum_version,
                                                                     uint32_t maximum_version,
                                                                     uint32_t* selected_version);
typedef size_t(CAORS_PLUGIN_CALL* caors_plugin_remove_services_fn)(void* context, void* owner);

/**
 * 借用型服务注册表。
 *
 * 服务地址由发布方拥有，并且必须保持有效，直到同一 owner 的记录被移除。服务 ID 是稳定 GUID，
 * 不得使用进程本地的 meta capability key。
 */
typedef struct caors_plugin_services_extension {
  uint32_t struct_size;
  uint32_t version;
  void* context;
  caors_plugin_publish_service_fn publish;
  caors_plugin_find_service_fn find;
  caors_plugin_remove_services_fn remove_owner;
} caors_plugin_services_extension;

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
