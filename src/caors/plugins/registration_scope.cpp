// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/registration_scope.h"

#include <algorithm>
#include <new>
#include <utility>
#include <vector>

namespace caors::plugins {

struct plugin_registration_scope::impl {
  plugin_host* host = nullptr;
  void* owner = nullptr;
  std::vector<events::connection> connections{};
  std::vector<tasks::request> requests{};
  std::size_t plugin_owned_values = 0;
  plugin_cleanup_result cleanup_result{};
  bool cleaned = false;
};

plugin_registration_scope::plugin_registration_scope(plugin_host& host, void* owner)
    : data_(new (std::nothrow) impl{&host, owner}) {}

plugin_registration_scope::~plugin_registration_scope() {
  (void)cleanup();
  delete data_;
}

error plugin_registration_scope::publish_service(guid id, std::uint32_t version,
                                                 const void* service) {
  if (data_ == nullptr) {
    return {error_domain::plugins, errc::out_of_memory};
  }
  if (data_->cleaned || data_->owner == nullptr) {
    return {error_domain::plugins, errc::invalid_argument};
  }
  return data_->host->publish_service(id, version, service, data_->owner);
}

error plugin_registration_scope::track(events::connection connection) {
  if (data_ == nullptr) {
    return {error_domain::plugins, errc::out_of_memory};
  }
  if (data_->cleaned || !connection) {
    return {error_domain::plugins, errc::invalid_argument};
  }
  try {
    data_->connections.push_back(std::move(connection));
  } catch (const std::bad_alloc&) {
    return {error_domain::plugins, errc::out_of_memory};
  }
  return {};
}

error plugin_registration_scope::track(tasks::request request) {
  if (data_ == nullptr) {
    return {error_domain::plugins, errc::out_of_memory};
  }
  if (data_->cleaned || !request) {
    return {error_domain::plugins, errc::invalid_argument};
  }
  try {
    data_->requests.push_back(std::move(request));
  } catch (const std::bad_alloc&) {
    return {error_domain::plugins, errc::out_of_memory};
  }
  return {};
}

void plugin_registration_scope::record_plugin_owned_value(std::size_t count) noexcept {
  if (data_ != nullptr && !data_->cleaned) {
    data_->plugin_owned_values += count;
  }
}

void plugin_registration_scope::release_plugin_owned_value(std::size_t count) noexcept {
  if (data_ != nullptr && !data_->cleaned) {
    data_->plugin_owned_values =
        count < data_->plugin_owned_values ? data_->plugin_owned_values - count : 0;
  }
}

plugin_cleanup_result plugin_registration_scope::cleanup() noexcept {
  if (data_ == nullptr) {
    return {{error_domain::plugins, errc::out_of_memory}};
  }
  if (data_->cleaned) {
    return data_->cleanup_result;
  }
  data_->cleaned = true;

  data_->cleanup_result.removed_services = data_->host->remove_services(data_->owner);
  for (events::connection& connection : data_->connections) {
    if (connection.disconnect()) {
      ++data_->cleanup_result.disconnected_events;
    }
  }
  for (const tasks::request& request : data_->requests) {
    if (request.done()) {
      continue;
    }
    if (request.cancel()) {
      ++data_->cleanup_result.cancelled_requests;
    }
    // cancel 只改变共享状态，不能证明 executor 已销毁插件 callable。
    data_->cleanup_result.blockers =
        data_->cleanup_result.blockers | plugin_unload_blocker::pending_task;
  }
  if (data_->plugin_owned_values != 0) {
    data_->cleanup_result.blockers =
        data_->cleanup_result.blockers | plugin_unload_blocker::plugin_owned_value;
  }
  if (data_->cleanup_result.blockers != plugin_unload_blocker::none) {
    data_->cleanup_result.status = {error_domain::plugins, errc::unsupported};
  }
  return data_->cleanup_result;
}

const plugin_cleanup_result& plugin_registration_scope::result() const noexcept {
  static const plugin_cleanup_result unavailable{
      {error_domain::plugins, errc::out_of_memory},
  };
  return data_ != nullptr ? data_->cleanup_result : unavailable;
}

} // namespace caors::plugins
