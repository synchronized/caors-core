// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/plugin_host.h"

#include "caors/version.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <vector>

namespace caors::plugins {
namespace {

struct extension_record {
  std::string id{};
  std::uint32_t version = 0;
  const void* value = nullptr;
};

struct service_record {
  guid id{};
  std::uint32_t version = 0;
  const void* value = nullptr;
  void* owner = nullptr;
};

struct registration_context_record {
  void* owner = nullptr;
  void* context = nullptr;
};

[[nodiscard]] guid copy_guid(const caors_plugin_guid& source) noexcept {
  guid::byte_array bytes{};
  std::copy(std::begin(source.bytes), std::end(source.bytes), bytes.begin());
  return guid::from_bytes(bytes);
}

} // namespace

struct plugin_host::impl {
  caors_plugin_host host{};
  caors_plugin_runtime_extension runtime{};
  caors_plugin_services_extension services{};
  caors_plugin_meta_registration_extension meta_registration{};
  caors_plugin_log_fn log = nullptr;
  void* log_context = nullptr;
  mutable std::mutex mutex{};
  std::vector<extension_record> extensions{};
  std::vector<service_record> service_records{};
  std::vector<registration_context_record> registration_contexts{};

  static void CAORS_PLUGIN_CALL write_log(void* context, std::uint32_t level, const char* message,
                                          std::size_t message_size) {
    auto& self = *static_cast<impl*>(context);
    if (self.log != nullptr) {
      try {
        self.log(self.log_context, level, message, message_size);
      } catch (...) {
        // C ABI 边界不允许传播异常。
      }
    }
  }

  static const void* CAORS_PLUGIN_CALL get_extension(void* context, const char* id,
                                                     std::size_t id_size, std::uint32_t version) {
    if (context == nullptr || id == nullptr) {
      return nullptr;
    }
    try {
      return static_cast<impl*>(context)->find_extension({id, id_size}, version);
    } catch (...) {
      return nullptr;
    }
  }

  static std::int32_t CAORS_PLUGIN_CALL publish_service(void* context, const caors_plugin_guid* id,
                                                        std::uint32_t version, const void* service,
                                                        void* owner) {
    if (context == nullptr || id == nullptr) {
      return CAORS_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    try {
      const error status =
          static_cast<impl*>(context)->publish_service(copy_guid(*id), version, service, owner);
      if (!status) {
        return CAORS_PLUGIN_STATUS_SUCCESS;
      }
      return status.value() == errc::already_exists ? CAORS_PLUGIN_STATUS_ALREADY_EXISTS
                                                    : CAORS_PLUGIN_STATUS_INVALID_ARGUMENT;
    } catch (const std::bad_alloc&) {
      return CAORS_PLUGIN_STATUS_OUT_OF_MEMORY;
    } catch (...) {
      return CAORS_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
  }

  static const void* CAORS_PLUGIN_CALL find_service(void* context, const caors_plugin_guid* id,
                                                    std::uint32_t minimum_version,
                                                    std::uint32_t maximum_version,
                                                    std::uint32_t* selected_version) {
    if (context == nullptr || id == nullptr) {
      return nullptr;
    }
    try {
      return static_cast<impl*>(context)->find_service(copy_guid(*id), minimum_version,
                                                       maximum_version, selected_version);
    } catch (...) {
      return nullptr;
    }
  }

  static std::size_t CAORS_PLUGIN_CALL remove_services(void* context, void* owner) {
    if (context == nullptr) {
      return 0;
    }
    try {
      return static_cast<impl*>(context)->remove_services(owner);
    } catch (...) {
      return 0;
    }
  }

  static void* CAORS_PLUGIN_CALL find_registration_context(void* context, void* owner) {
    if (context == nullptr || owner == nullptr) {
      return nullptr;
    }
    try {
      auto& self = *static_cast<impl*>(context);
      const std::lock_guard lock{self.mutex};
      const auto found =
          std::find_if(self.registration_contexts.begin(), self.registration_contexts.end(),
                       [owner](const auto& item) { return item.owner == owner; });
      return found != self.registration_contexts.end() ? found->context : nullptr;
    } catch (...) {
      return nullptr;
    }
  }

  [[nodiscard]] const void* find_extension(std::string_view id,
                                           std::uint32_t version) const noexcept {
    if (id == CAORS_PLUGIN_RUNTIME_EXTENSION_ID &&
        version == CAORS_PLUGIN_RUNTIME_EXTENSION_VERSION) {
      return &runtime;
    }
    if (id == CAORS_PLUGIN_SERVICES_EXTENSION_ID &&
        version == CAORS_PLUGIN_SERVICES_EXTENSION_VERSION) {
      return &services;
    }
    if (id == CAORS_PLUGIN_META_REGISTRATION_EXTENSION_ID &&
        version == CAORS_PLUGIN_META_REGISTRATION_EXTENSION_VERSION) {
      return &meta_registration;
    }
    const std::lock_guard lock{mutex};
    const auto found = std::find_if(extensions.begin(), extensions.end(), [&](const auto& item) {
      return item.id == id && item.version == version;
    });
    return found != extensions.end() ? found->value : nullptr;
  }

  [[nodiscard]] error publish_service(guid id, std::uint32_t version, const void* service,
                                      void* owner) {
    if (!id || version == 0 || service == nullptr || owner == nullptr) {
      return {error_domain::plugins, errc::invalid_argument};
    }
    const std::lock_guard lock{mutex};
    const auto duplicate =
        std::find_if(service_records.begin(), service_records.end(),
                     [&](const auto& item) { return item.id == id && item.version == version; });
    if (duplicate != service_records.end()) {
      return {error_domain::plugins, errc::already_exists};
    }
    service_records.push_back({id, version, service, owner});
    return {};
  }

  [[nodiscard]] const void* find_service(guid id, std::uint32_t minimum_version,
                                         std::uint32_t maximum_version,
                                         std::uint32_t* selected_version) const noexcept {
    if (!id || minimum_version > maximum_version) {
      return nullptr;
    }
    const std::lock_guard lock{mutex};
    const service_record* selected = nullptr;
    for (const service_record& item : service_records) {
      if (item.id == id && item.version >= minimum_version && item.version <= maximum_version &&
          (selected == nullptr || item.version > selected->version)) {
        selected = &item;
      }
    }
    if (selected_version != nullptr) {
      *selected_version = selected != nullptr ? selected->version : 0;
    }
    return selected != nullptr ? selected->value : nullptr;
  }

  std::size_t remove_services(void* owner) noexcept {
    if (owner == nullptr) {
      return 0;
    }
    const std::lock_guard lock{mutex};
    const std::size_t previous_size = service_records.size();
    std::erase_if(service_records,
                  [owner](const service_record& item) { return item.owner == owner; });
    return previous_size - service_records.size();
  }
};

plugin_host::plugin_host() : data_(new (std::nothrow) impl{}) {
  if (data_ == nullptr) {
    return;
  }
  data_->runtime = {
      sizeof(caors_plugin_runtime_extension),      CAORS_PLUGIN_RUNTIME_EXTENSION_VERSION,
      static_cast<std::uint32_t>(k_version.major), static_cast<std::uint32_t>(k_version.minor),
      static_cast<std::uint32_t>(k_version.patch), CAORS_PLUGIN_BUILD_ENVIRONMENT_INITIALIZER};
  data_->services = {sizeof(caors_plugin_services_extension),
                     CAORS_PLUGIN_SERVICES_EXTENSION_VERSION,
                     data_,
                     &impl::publish_service,
                     &impl::find_service,
                     &impl::remove_services};
  data_->meta_registration = {sizeof(caors_plugin_meta_registration_extension),
                              CAORS_PLUGIN_META_REGISTRATION_EXTENSION_VERSION, data_,
                              &impl::find_registration_context};
  data_->host = {sizeof(caors_plugin_host), CAORS_PLUGIN_ABI_VERSION, data_, &impl::write_log,
                 &impl::get_extension};
}

plugin_host::~plugin_host() { delete data_; }

const caors_plugin_host& plugin_host::abi() const noexcept {
  static const caors_plugin_host empty{};
  return data_ != nullptr ? data_->host : empty;
}

void plugin_host::set_log(caors_plugin_log_fn callback, void* context) noexcept {
  if (data_ != nullptr) {
    data_->log = callback;
    data_->log_context = context;
  }
}

error plugin_host::add_extension(std::string_view id, std::uint32_t version,
                                 const void* extension) {
  if (data_ == nullptr) {
    return {error_domain::plugins, errc::out_of_memory};
  }
  if (id.empty() || id.find('\0') != std::string_view::npos || version == 0 ||
      extension == nullptr || id == CAORS_PLUGIN_RUNTIME_EXTENSION_ID ||
      id == CAORS_PLUGIN_SERVICES_EXTENSION_ID ||
      id == CAORS_PLUGIN_META_REGISTRATION_EXTENSION_ID) {
    return {error_domain::plugins, errc::invalid_argument};
  }
  const std::lock_guard lock{data_->mutex};
  const auto duplicate =
      std::find_if(data_->extensions.begin(), data_->extensions.end(),
                   [&](const auto& item) { return item.id == id && item.version == version; });
  if (duplicate != data_->extensions.end()) {
    return {error_domain::plugins, errc::already_exists};
  }
  data_->extensions.push_back({std::string{id}, version, extension});
  return {};
}

const void* plugin_host::find_extension(std::string_view id, std::uint32_t version) const noexcept {
  return data_ != nullptr ? data_->find_extension(id, version) : nullptr;
}

error plugin_host::publish_service(guid id, std::uint32_t version, const void* service,
                                   void* owner) {
  return data_ != nullptr ? data_->publish_service(id, version, service, owner)
                          : error{error_domain::plugins, errc::out_of_memory};
}

const void* plugin_host::find_service(guid id, std::uint32_t minimum_version,
                                      std::uint32_t maximum_version,
                                      std::uint32_t* selected_version) const noexcept {
  return data_ != nullptr
             ? data_->find_service(id, minimum_version, maximum_version, selected_version)
             : nullptr;
}

std::size_t plugin_host::remove_services(void* owner) noexcept {
  return data_ != nullptr ? data_->remove_services(owner) : 0;
}

error plugin_host::bind_registration_context(void* owner, void* context) {
  if (data_ == nullptr) {
    return {error_domain::plugins, errc::out_of_memory};
  }
  if (owner == nullptr || context == nullptr) {
    return {error_domain::plugins, errc::invalid_argument};
  }
  const std::lock_guard lock{data_->mutex};
  const auto duplicate =
      std::find_if(data_->registration_contexts.begin(), data_->registration_contexts.end(),
                   [owner](const auto& item) { return item.owner == owner; });
  if (duplicate != data_->registration_contexts.end()) {
    return {error_domain::plugins, errc::already_exists};
  }
  data_->registration_contexts.push_back({owner, context});
  return {};
}

void plugin_host::unbind_registration_context(void* owner) noexcept {
  if (data_ == nullptr || owner == nullptr) {
    return;
  }
  const std::lock_guard lock{data_->mutex};
  std::erase_if(data_->registration_contexts,
                [owner](const auto& item) { return item.owner == owner; });
}

} // namespace caors::plugins
