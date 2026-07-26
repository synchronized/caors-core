// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/plugin_binary.h"

#include "caors/meta/registry.h"
#include "caors/platform/shared_library.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <new>
#include <utility>

namespace caors::plugins {
namespace {

constexpr std::size_t minimum_descriptor_size =
    offsetof(caors_plugin_descriptor, version) + sizeof(caors_plugin_version);
constexpr std::size_t minimum_entry_size =
    offsetof(caors_plugin_entry, shutdown) + sizeof(caors_plugin_lifecycle_fn);

[[nodiscard]] std::string copy_string(caors_plugin_string_view source) {
  return source.data != nullptr ? std::string{source.data, source.size} : std::string{};
}

[[nodiscard]] guid copy_guid(const caors_plugin_guid& source) noexcept {
  guid::byte_array bytes{};
  std::copy(std::begin(source.bytes), std::end(source.bytes), bytes.begin());
  return guid::from_bytes(bytes);
}

[[nodiscard]] error lifecycle_result(std::int32_t status) noexcept {
  return status == 0 ? error{} : error{error_domain::plugins, errc::backend_failure};
}

} // namespace

struct plugin_library_lease::impl {
  explicit impl(platform::shared_library source) noexcept : library(std::move(source)) {}

  platform::shared_library library{};
};

struct plugin_binary::impl {
  std::shared_ptr<plugin_library_lease::impl> library{};
  caors_plugin_host host{};
  const caors_plugin_entry* entry = nullptr;
  plugins::plugin_descriptor descriptor{};
  plugin_state state = plugin_state::loaded;
};

plugin_binary::plugin_binary(plugin_binary&& other) noexcept
    : data_(std::exchange(other.data_, nullptr)) {}

plugin_binary& plugin_binary::operator=(plugin_binary&& other) noexcept {
  if (this != &other) {
    delete data_;
    data_ = std::exchange(other.data_, nullptr);
  }
  return *this;
}

plugin_binary::~plugin_binary() { delete data_; }

result<plugin_binary> plugin_binary::load(const std::filesystem::path& path,
                                          const caors_plugin_host* host) {
  auto library_result = platform::shared_library::open(path);
  if (!library_result) {
    return fail<plugin_binary>(rethrow_as(library_result.error(), error_domain::plugins));
  }

  platform::shared_library library = std::move(*library_result);
  const auto query = library.find_function<caors_query_plugin_fn>(CAORS_PLUGIN_ENTRY_SYMBOL);
  if (query == nullptr) {
    return fail<plugin_binary>({error_domain::plugins, errc::not_found});
  }

  const caors_plugin_host resolved_host =
      host != nullptr ? *host
                      : caors_plugin_host{sizeof(caors_plugin_host), CAORS_PLUGIN_ABI_VERSION,
                                          nullptr, nullptr, nullptr};
  if (resolved_host.struct_size < sizeof(caors_plugin_host) ||
      resolved_host.abi_version != CAORS_PLUGIN_ABI_VERSION) {
    return fail<plugin_binary>({error_domain::plugins, errc::inconsistent_type});
  }

  auto library_control = std::shared_ptr<plugin_library_lease::impl>{
      new (std::nothrow) plugin_library_lease::impl{std::move(library)}};
  if (library_control == nullptr) {
    return fail<plugin_binary>({error_domain::plugins, errc::out_of_memory});
  }

  auto* data = new (std::nothrow)
      impl{std::move(library_control), resolved_host, nullptr, {}, plugin_state::loaded};
  if (data == nullptr) {
    return fail<plugin_binary>({error_domain::plugins, errc::out_of_memory});
  }
  std::unique_ptr<impl> data_owner{data};

  const caors_plugin_entry* entry = query(&data->host, CAORS_PLUGIN_ABI_VERSION);
  if (entry == nullptr || entry->struct_size < minimum_entry_size ||
      entry->abi_version != CAORS_PLUGIN_ABI_VERSION || entry->descriptor == nullptr) {
    return fail<plugin_binary>({error_domain::plugins, errc::inconsistent_type});
  }

  const caors_plugin_descriptor& source = *entry->descriptor;
  if (source.struct_size < minimum_descriptor_size) {
    return fail<plugin_binary>({error_domain::plugins, errc::inconsistent_type});
  }

  data->descriptor = plugin_descriptor{
      copy_guid(source.id),
      copy_string(source.name),
      copy_string(source.display_name),
      {static_cast<int>(source.version.major), static_cast<int>(source.version.minor),
       static_cast<int>(source.version.patch)},
  };
  if (!data->descriptor.valid()) {
    return fail<plugin_binary>({error_domain::plugins, errc::invalid_type});
  }

  data->entry = entry;
  return ok(plugin_binary{data_owner.release()});
}

bool plugin_binary::valid() const noexcept { return data_ != nullptr; }

plugin_state plugin_binary::state() const noexcept {
  return data_ != nullptr ? data_->state : plugin_state::unloaded;
}

const plugin_descriptor& plugin_binary::descriptor() const noexcept {
  static const plugin_descriptor empty{};
  return data_ != nullptr ? data_->descriptor : empty;
}

const std::filesystem::path& plugin_binary::path() const noexcept {
  static const std::filesystem::path empty{};
  return data_ != nullptr && data_->library != nullptr ? data_->library->library.path() : empty;
}

void* plugin_binary::owner_token() const noexcept {
  return data_ != nullptr && data_->entry != nullptr ? data_->entry->plugin_data : nullptr;
}

plugin_library_lease plugin_binary::library_lease() const noexcept {
  return data_ != nullptr ? plugin_library_lease{data_->library} : plugin_library_lease{};
}

error plugin_binary::initialize() noexcept {
  if (data_ == nullptr || data_->state != plugin_state::loaded ||
      data_->entry->initialize == nullptr) {
    return {error_domain::plugins, errc::invalid_argument};
  }
  const error status = lifecycle_result(data_->entry->initialize(data_->entry->plugin_data));
  data_->state = status ? plugin_state::failed : plugin_state::initialized;
  return status;
}

error plugin_binary::start() noexcept {
  if (data_ == nullptr || data_->state != plugin_state::initialized ||
      data_->entry->start == nullptr) {
    return {error_domain::plugins, errc::invalid_argument};
  }
  const error status = lifecycle_result(data_->entry->start(data_->entry->plugin_data));
  data_->state = status ? plugin_state::failed : plugin_state::running;
  return status;
}

error plugin_binary::stop() noexcept {
  if (data_ == nullptr || data_->state != plugin_state::running || data_->entry->stop == nullptr) {
    return {error_domain::plugins, errc::invalid_argument};
  }
  const error status = lifecycle_result(data_->entry->stop(data_->entry->plugin_data));
  data_->state = status ? plugin_state::failed : plugin_state::stopped;
  return status;
}

error plugin_binary::shutdown() noexcept {
  if (data_ == nullptr ||
      (data_->state != plugin_state::initialized && data_->state != plugin_state::stopped &&
       data_->state != plugin_state::failed) ||
      data_->entry->shutdown == nullptr) {
    return {error_domain::plugins, errc::invalid_argument};
  }
  const error status = lifecycle_result(data_->entry->shutdown(data_->entry->plugin_data));
  data_->state = status ? plugin_state::failed : plugin_state::loaded;
  return status;
}

const void* plugin_binary::get_extension(std::string_view id,
                                         std::uint32_t version) const noexcept {
  if (data_ == nullptr || id.empty() || id.find('\0') != std::string_view::npos ||
      data_->entry->get_extension == nullptr) {
    return nullptr;
  }
  return data_->entry->get_extension(data_->entry->plugin_data, id.data(), id.size(), version);
}

bool plugin_library_lease::valid() const noexcept {
  return data_ != nullptr && data_->library.loaded();
}

const std::filesystem::path& plugin_library_lease::path() const noexcept {
  static const std::filesystem::path empty{};
  return valid() ? data_->library.path() : empty;
}

std::size_t plugin_library_lease::use_count() const noexcept {
  return data_ != nullptr ? data_.use_count() : 0;
}

error plugin_library_lease::bind(meta::registry_layer& layer) const {
  if (!valid()) {
    return {error_domain::plugins, errc::invalid_argument};
  }
  return layer.attach_lifetime(data_);
}

} // namespace caors::plugins
