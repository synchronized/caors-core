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
#define CAORS_PLUGIN_RUNTIME_EXTENSION_VERSION 2u
#define CAORS_PLUGIN_BUILD_EXTENSION_ID "caors.plugin.build"
#define CAORS_PLUGIN_BUILD_EXTENSION_VERSION 1u
#define CAORS_PLUGIN_SERVICES_EXTENSION_ID "caors.host.services"
#define CAORS_PLUGIN_SERVICES_EXTENSION_VERSION 1u
#define CAORS_PLUGIN_META_REGISTRATION_EXTENSION_ID "caors.host.meta-registration"
#define CAORS_PLUGIN_META_REGISTRATION_EXTENSION_VERSION 1u

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

typedef enum caors_plugin_compiler_family {
  CAORS_PLUGIN_COMPILER_UNKNOWN = 0,
  CAORS_PLUGIN_COMPILER_MSVC = 1,
  CAORS_PLUGIN_COMPILER_CLANG_CL = 2,
  CAORS_PLUGIN_COMPILER_CLANG = 3,
  CAORS_PLUGIN_COMPILER_GCC = 4
} caors_plugin_compiler_family;

typedef enum caors_plugin_crt_mode {
  CAORS_PLUGIN_CRT_UNKNOWN = 0,
  CAORS_PLUGIN_CRT_DYNAMIC_RELEASE = 1,
  CAORS_PLUGIN_CRT_DYNAMIC_DEBUG = 2,
  CAORS_PLUGIN_CRT_STATIC_RELEASE = 3,
  CAORS_PLUGIN_CRT_STATIC_DEBUG = 4
} caors_plugin_crt_mode;

typedef enum caors_plugin_architecture {
  CAORS_PLUGIN_ARCHITECTURE_UNKNOWN = 0,
  CAORS_PLUGIN_ARCHITECTURE_X86 = 1,
  CAORS_PLUGIN_ARCHITECTURE_X64 = 2,
  CAORS_PLUGIN_ARCHITECTURE_ARM32 = 3,
  CAORS_PLUGIN_ARCHITECTURE_ARM64 = 4
} caors_plugin_architecture;

#if defined(__clang__) && defined(_MSC_VER)
#define CAORS_PLUGIN_CURRENT_COMPILER CAORS_PLUGIN_COMPILER_CLANG_CL
#define CAORS_PLUGIN_CURRENT_COMPILER_VERSION                                                      \
  (((uint32_t)__clang_major__ << 16u) | ((uint32_t)__clang_minor__ << 8u) |                        \
   (uint32_t)__clang_patchlevel__)
#define CAORS_PLUGIN_CURRENT_ABI_VERSION ((uint32_t)_MSC_VER)
#elif defined(_MSC_VER)
#define CAORS_PLUGIN_CURRENT_COMPILER CAORS_PLUGIN_COMPILER_MSVC
#define CAORS_PLUGIN_CURRENT_COMPILER_VERSION ((uint32_t)_MSC_FULL_VER)
#define CAORS_PLUGIN_CURRENT_ABI_VERSION ((uint32_t)_MSC_VER)
#elif defined(__clang__)
#define CAORS_PLUGIN_CURRENT_COMPILER CAORS_PLUGIN_COMPILER_CLANG
#define CAORS_PLUGIN_CURRENT_COMPILER_VERSION                                                      \
  (((uint32_t)__clang_major__ << 16u) | ((uint32_t)__clang_minor__ << 8u) |                        \
   (uint32_t)__clang_patchlevel__)
#define CAORS_PLUGIN_CURRENT_ABI_VERSION 0u
#elif defined(__GNUC__)
#define CAORS_PLUGIN_CURRENT_COMPILER CAORS_PLUGIN_COMPILER_GCC
#define CAORS_PLUGIN_CURRENT_COMPILER_VERSION                                                      \
  (((uint32_t)__GNUC__ << 16u) | ((uint32_t)__GNUC_MINOR__ << 8u) | (uint32_t)__GNUC_PATCHLEVEL__)
#define CAORS_PLUGIN_CURRENT_ABI_VERSION 0u
#else
#define CAORS_PLUGIN_CURRENT_COMPILER CAORS_PLUGIN_COMPILER_UNKNOWN
#define CAORS_PLUGIN_CURRENT_COMPILER_VERSION 0u
#define CAORS_PLUGIN_CURRENT_ABI_VERSION 0u
#endif

#if defined(_WIN32) && defined(_DLL) && defined(_DEBUG)
#define CAORS_PLUGIN_CURRENT_CRT_MODE CAORS_PLUGIN_CRT_DYNAMIC_DEBUG
#elif defined(_WIN32) && defined(_DLL)
#define CAORS_PLUGIN_CURRENT_CRT_MODE CAORS_PLUGIN_CRT_DYNAMIC_RELEASE
#elif defined(_WIN32) && defined(_DEBUG)
#define CAORS_PLUGIN_CURRENT_CRT_MODE CAORS_PLUGIN_CRT_STATIC_DEBUG
#elif defined(_WIN32)
#define CAORS_PLUGIN_CURRENT_CRT_MODE CAORS_PLUGIN_CRT_STATIC_RELEASE
#else
#define CAORS_PLUGIN_CURRENT_CRT_MODE CAORS_PLUGIN_CRT_UNKNOWN
#endif

#if defined(_ITERATOR_DEBUG_LEVEL)
#define CAORS_PLUGIN_CURRENT_ITERATOR_DEBUG_LEVEL ((uint32_t)_ITERATOR_DEBUG_LEVEL)
#elif defined(_MSC_VER) && defined(_DEBUG)
#define CAORS_PLUGIN_CURRENT_ITERATOR_DEBUG_LEVEL 2u
#else
#define CAORS_PLUGIN_CURRENT_ITERATOR_DEBUG_LEVEL 0u
#endif

#if defined(_M_X64) || defined(__x86_64__)
#define CAORS_PLUGIN_CURRENT_ARCHITECTURE CAORS_PLUGIN_ARCHITECTURE_X64
#elif defined(_M_IX86) || defined(__i386__)
#define CAORS_PLUGIN_CURRENT_ARCHITECTURE CAORS_PLUGIN_ARCHITECTURE_X86
#elif defined(_M_ARM64) || defined(__aarch64__)
#define CAORS_PLUGIN_CURRENT_ARCHITECTURE CAORS_PLUGIN_ARCHITECTURE_ARM64
#elif defined(_M_ARM) || defined(__arm__)
#define CAORS_PLUGIN_CURRENT_ARCHITECTURE CAORS_PLUGIN_ARCHITECTURE_ARM32
#else
#define CAORS_PLUGIN_CURRENT_ARCHITECTURE CAORS_PLUGIN_ARCHITECTURE_UNKNOWN
#endif

typedef struct caors_plugin_build_environment {
  uint32_t compiler_family;
  uint32_t compiler_version;
  uint32_t compiler_abi_version;
  uint32_t crt_mode;
  uint32_t iterator_debug_level;
  uint32_t pointer_size;
  uint32_t architecture;
} caors_plugin_build_environment;

#define CAORS_PLUGIN_BUILD_ENVIRONMENT_INITIALIZER                                                 \
  {CAORS_PLUGIN_CURRENT_COMPILER,                                                                  \
   CAORS_PLUGIN_CURRENT_COMPILER_VERSION,                                                          \
   CAORS_PLUGIN_CURRENT_ABI_VERSION,                                                               \
   CAORS_PLUGIN_CURRENT_CRT_MODE,                                                                  \
   CAORS_PLUGIN_CURRENT_ITERATOR_DEBUG_LEVEL,                                                      \
   (uint32_t)sizeof(void*),                                                                        \
   CAORS_PLUGIN_CURRENT_ARCHITECTURE}

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
  caors_plugin_build_environment environment;
} caors_plugin_runtime_extension;

typedef struct caors_plugin_build_extension {
  uint32_t struct_size;
  uint32_t version;
  caors_plugin_build_environment environment;
} caors_plugin_build_extension;

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

typedef void*(CAORS_PLUGIN_CALL* caors_plugin_find_registration_context_fn)(void* context,
                                                                            void* owner);

/**
 * 查询插件 initialize 阶段专属的 opaque meta registration context。
 *
 * owner 必须是当前插件入口中的 plugin_data。返回值只在 initialize 调用期间有效，不得保存。
 */
typedef struct caors_plugin_meta_registration_extension {
  uint32_t struct_size;
  uint32_t version;
  void* context;
  caors_plugin_find_registration_context_fn find;
} caors_plugin_meta_registration_extension;

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
