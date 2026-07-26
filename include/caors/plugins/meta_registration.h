// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_PLUGINS_META_REGISTRATION_H_
#define CAORS_PLUGINS_META_REGISTRATION_H_

#include "caors/meta/registry.h"
#include "caors/plugins/abi.h"

namespace caors::plugins {

/**
 * @brief 查询当前插件 initialize 阶段可写的 registry layer。
 *
 * 返回指针不拥有 registry，且只允许在 initialize 返回前使用。
 */
[[nodiscard]] inline meta::registry* find_registration_registry(const caors_plugin_host* host,
                                                                void* owner) noexcept {
  if (host == nullptr || host->get_extension == nullptr || owner == nullptr) {
    return nullptr;
  }
  const auto* extension = static_cast<const caors_plugin_meta_registration_extension*>(
      host->get_extension(host->context, CAORS_PLUGIN_META_REGISTRATION_EXTENSION_ID,
                          sizeof(CAORS_PLUGIN_META_REGISTRATION_EXTENSION_ID) - 1,
                          CAORS_PLUGIN_META_REGISTRATION_EXTENSION_VERSION));
  if (extension == nullptr ||
      extension->struct_size < sizeof(caors_plugin_meta_registration_extension) ||
      extension->version != CAORS_PLUGIN_META_REGISTRATION_EXTENSION_VERSION ||
      extension->find == nullptr) {
    return nullptr;
  }
  return static_cast<meta::registry*>(extension->find(extension->context, owner));
}

} // namespace caors::plugins

#endif
