// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/plugin_manager.h"

#include "caors/meta/registry.h"
#include "caors/plugins/registration_scope.h"

#include <atomic>
#include <mutex>
#include <new>
#include <unordered_map>
#include <utility>
#include <vector>

namespace caors::plugins {
namespace {

std::atomic<std::uint64_t> next_plugin_generation{1};

} // namespace

namespace detail {

class managed_plugin final {
public:
  mutable std::mutex mutex{};
  plugin_descriptor descriptor{};
  std::filesystem::path path{};
  std::uint64_t generation = 0;
  plugin_state state = plugin_state::unloaded;
  plugin_lifetime_policy lifetime = plugin_lifetime_policy::detachable;
  plugin_unload_diagnostic unload_diagnostic{};
  bool busy = false;
  bool layer_published = false;
  plugin_binary binary{};
  std::unique_ptr<meta::registry_layer> layer{};
  std::unique_ptr<plugin_registration_scope> scope{};
};

} // namespace detail

struct plugin_manager::impl {
  explicit impl(meta::registry& target) : registry(&target) {}

  meta::registry* registry = nullptr;
  plugin_host host{};
  mutable std::mutex mutex{};
  std::unordered_map<guid, std::shared_ptr<detail::managed_plugin>, guid_hash> plugins{};
};

namespace {

template <typename Data>
[[nodiscard]] std::shared_ptr<detail::managed_plugin> find_node(Data* data, guid id) {
  if (data == nullptr || !id) {
    return {};
  }
  std::lock_guard lock{data->mutex};
  const auto position = data->plugins.find(id);
  return position != data->plugins.end() ? position->second : nullptr;
}

[[nodiscard]] error begin_operation(const std::shared_ptr<detail::managed_plugin>& node,
                                    plugin_state expected) {
  if (node == nullptr) {
    return {error_domain::plugins, errc::not_found};
  }
  std::lock_guard lock{node->mutex};
  if (node->busy || node->state != expected) {
    return {error_domain::plugins, node->busy ? errc::pending : errc::invalid_argument};
  }
  node->busy = true;
  return {};
}

void finish_operation(const std::shared_ptr<detail::managed_plugin>& node,
                      plugin_state state) noexcept {
  std::lock_guard lock{node->mutex};
  node->state = state;
  node->busy = false;
}

[[nodiscard]] plugin_unload_diagnostic
make_unload_diagnostic(const detail::managed_plugin& node,
                       plugin_unload_blocker blockers = plugin_unload_blocker::none) noexcept {
  plugin_unload_diagnostic diagnostic{};
  diagnostic.id = node.descriptor.id;
  diagnostic.generation = node.generation;
  diagnostic.state = node.state;
  diagnostic.lifetime = node.lifetime;
  diagnostic.blockers = blockers;
  diagnostic.layer_published = node.layer_published;
  if (node.layer != nullptr) {
    diagnostic.layer_lease_count = node.layer->lease_count();
    diagnostic.in_flight_calls = node.layer->in_flight_calls();
    diagnostic.local_type_count = node.layer->local_size();
  }
  return diagnostic;
}

void store_unload_diagnostic(detail::managed_plugin& node,
                             plugin_unload_blocker blockers = plugin_unload_blocker::none) {
  node.unload_diagnostic = make_unload_diagnostic(node, blockers);
}

} // namespace

bool plugin::valid() const noexcept {
  if (data_ == nullptr) {
    return false;
  }
  std::lock_guard lock{data_->mutex};
  return data_->state != plugin_state::unloaded;
}

guid plugin::id() const noexcept { return data_ != nullptr ? data_->descriptor.id : guid{}; }

std::uint64_t plugin::generation() const noexcept {
  return data_ != nullptr ? data_->generation : 0;
}

plugin_state plugin::state() const noexcept {
  if (data_ == nullptr) {
    return plugin_state::unloaded;
  }
  std::lock_guard lock{data_->mutex};
  return data_->state;
}

plugin_lifetime_policy plugin::lifetime() const noexcept {
  if (data_ == nullptr) {
    return plugin_lifetime_policy::detachable;
  }
  std::lock_guard lock{data_->mutex};
  return data_->lifetime;
}

plugin_unload_diagnostic plugin::unload_diagnostic() const noexcept {
  if (data_ == nullptr) {
    return {};
  }
  std::lock_guard lock{data_->mutex};
  return data_->unload_diagnostic;
}

const plugin_descriptor& plugin::descriptor() const noexcept {
  static const plugin_descriptor empty{};
  return data_ != nullptr ? data_->descriptor : empty;
}

const std::filesystem::path& plugin::path() const noexcept {
  static const std::filesystem::path empty{};
  return data_ != nullptr ? data_->path : empty;
}

const void* plugin::get_extension(std::string_view id, std::uint32_t version) const noexcept {
  if (data_ == nullptr) {
    return nullptr;
  }
  {
    std::lock_guard lock{data_->mutex};
    if (data_->busy || data_->state == plugin_state::unloaded) {
      return nullptr;
    }
    data_->busy = true;
  }
  const void* extension = data_->binary.get_extension(id, version);
  {
    std::lock_guard lock{data_->mutex};
    data_->busy = false;
  }
  return extension;
}

plugin_manager::plugin_manager(meta::registry& registry)
    : data_(new (std::nothrow) impl{registry}) {}

plugin_manager::~plugin_manager() {
  if (data_ != nullptr) {
    std::vector<guid> ids{};
    {
      std::lock_guard lock{data_->mutex};
      ids.reserve(data_->plugins.size());
      for (const auto& [id, node] : data_->plugins) {
        (void)node;
        ids.push_back(id);
      }
    }
    for (const guid id : ids) {
      (void)unload(id);
    }
  }
  delete data_;
}

plugin_host& plugin_manager::host() noexcept { return data_->host; }

const plugin_host& plugin_manager::host() const noexcept { return data_->host; }

result<plugin> plugin_manager::load(const std::filesystem::path& path,
                                    plugin_load_options options) {
  if (data_ == nullptr) {
    return fail<plugin>({error_domain::plugins, errc::out_of_memory});
  }

  auto loaded = plugin_binary::load(path, &data_->host.abi());
  if (!loaded) {
    return fail<plugin>(loaded.error());
  }
  {
    std::lock_guard lock{data_->mutex};
    if (data_->plugins.contains(loaded->descriptor().id)) {
      return fail<plugin>({error_domain::plugins, errc::duplicate_id});
    }
  }

  auto node = std::shared_ptr<detail::managed_plugin>{new (std::nothrow) detail::managed_plugin{}};
  if (node == nullptr) {
    return fail<plugin>({error_domain::plugins, errc::out_of_memory});
  }
  node->binary = std::move(*loaded);
  node->descriptor = node->binary.descriptor();
  node->path = node->binary.path();
  node->generation = next_plugin_generation.fetch_add(1, std::memory_order_relaxed);
  node->state = plugin_state::loaded;
  node->lifetime = options.lifetime;
  node->layer =
      data_->registry->create_layer(options.lifetime == plugin_lifetime_policy::pinned_process
                                        ? meta::registry_layer_lifetime::pinned_process
                                        : meta::registry_layer_lifetime::detachable);
  if (node->layer == nullptr) {
    return fail<plugin>({error_domain::plugins, errc::out_of_memory});
  }
  const error bind_status = node->binary.library_lease().bind(*node->layer);
  if (bind_status) {
    return fail<plugin>(bind_status);
  }
  node->scope = std::unique_ptr<plugin_registration_scope>{
      new (std::nothrow) plugin_registration_scope{data_->host, node.get()}};
  if (node->scope == nullptr) {
    return fail<plugin>({error_domain::plugins, errc::out_of_memory});
  }

  {
    std::lock_guard lock{data_->mutex};
    if (data_->plugins.contains(node->descriptor.id)) {
      return fail<plugin>({error_domain::plugins, errc::duplicate_id});
    }
    data_->plugins.emplace(node->descriptor.id, node);
  }
  return ok(plugin{std::move(node)});
}

plugin plugin_manager::find(guid id) const noexcept { return plugin{find_node(data_, id)}; }

std::size_t plugin_manager::size() const noexcept {
  if (data_ == nullptr) {
    return 0;
  }
  std::lock_guard lock{data_->mutex};
  return data_->plugins.size();
}

error plugin_manager::initialize(guid id) {
  const auto node = find_node(data_, id);
  error status = begin_operation(node, plugin_state::loaded);
  if (status) {
    return status;
  }

  void* const owner = node->binary.owner_token();
  if (owner != nullptr) {
    status = data_->host.bind_registration_context(owner, &node->layer->get_registry());
    if (status) {
      finish_operation(node, plugin_state::loaded);
      return status;
    }
  }
  status = node->binary.initialize();
  data_->host.unbind_registration_context(owner);
  if (!status && node->layer->local_size() != 0) {
    status = data_->registry->publish_layer(*node->layer);
    node->layer_published = !status;
  }
  if (status) {
    (void)node->binary.shutdown();
    node->scope.reset();
    node->layer.reset();
    finish_operation(node, plugin_state::failed);
    return status;
  }

  finish_operation(node, plugin_state::initialized);
  return {};
}

error plugin_manager::start(guid id) {
  const auto node = find_node(data_, id);
  const error begin = begin_operation(node, plugin_state::initialized);
  if (begin) {
    return begin;
  }
  const error status = node->binary.start();
  finish_operation(node, status ? plugin_state::failed : plugin_state::running);
  return status;
}

error plugin_manager::stop(guid id) {
  const auto node = find_node(data_, id);
  const error begin = begin_operation(node, plugin_state::running);
  if (begin) {
    return begin;
  }
  const error status = node->binary.stop();
  finish_operation(node, status ? plugin_state::failed : plugin_state::stopped);
  return status;
}

error plugin_manager::unload(guid id) {
  const auto node = find_node(data_, id);
  if (node == nullptr) {
    return {error_domain::plugins, errc::not_found};
  }

  plugin_state state{};
  {
    std::lock_guard lock{node->mutex};
    if (node->busy) {
      store_unload_diagnostic(*node);
      return {error_domain::plugins, errc::pending};
    }
    node->busy = true;
    state = node->state;
  }

  if (node->lifetime == plugin_lifetime_policy::pinned_process) {
    {
      std::lock_guard lock{node->mutex};
      store_unload_diagnostic(*node, plugin_unload_blocker::pinned_process);
      node->busy = false;
    }
    return {error_domain::plugins, errc::unsupported};
  }

  error status{};
  if (state == plugin_state::running) {
    status = node->binary.stop();
    state = status ? plugin_state::failed : plugin_state::stopped;
  }
  if (!status &&
      (state == plugin_state::initialized || state == plugin_state::stopped ||
       (state == plugin_state::failed && node->binary.state() != plugin_state::loaded))) {
    status = node->binary.shutdown();
    state = status ? plugin_state::failed : plugin_state::loaded;
  }
  if (status) {
    finish_operation(node, plugin_state::failed);
    return status;
  }

  if (node->scope != nullptr) {
    const plugin_cleanup_result cleanup = node->scope->cleanup();
    if (!cleanup.can_unload()) {
      {
        std::lock_guard lock{node->mutex};
        node->state = plugin_state::detached_pending_release;
        store_unload_diagnostic(*node, cleanup.blockers);
        node->busy = false;
      }
      return {error_domain::plugins, errc::pending};
    }
  }
  if (node->layer_published) {
    status = data_->registry->detach_layer(*node->layer);
    if (status && status.value() != errc::not_found) {
      finish_operation(node, plugin_state::failed);
      return status;
    }
    node->layer_published = false;
  }
  if (node->layer != nullptr &&
      (node->layer->in_flight_calls() != 0 || node->layer->lease_count() > 2)) {
    {
      std::lock_guard lock{node->mutex};
      node->state = plugin_state::detached_pending_release;
      store_unload_diagnostic(*node);
      node->busy = false;
    }
    return {error_domain::plugins, errc::pending};
  }

  node->scope.reset();
  node->layer.reset();
  node->binary = {};
  finish_operation(node, plugin_state::unloaded);
  {
    std::lock_guard lock{data_->mutex};
    const auto position = data_->plugins.find(id);
    if (position != data_->plugins.end() && position->second == node) {
      data_->plugins.erase(position);
    }
  }
  return {};
}

} // namespace caors::plugins
