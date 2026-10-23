// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/resources/source.h"
#include "caors/io/file_system.h"

#include <utility>

namespace caors::resources {
namespace {

[[nodiscard]] core::string make_location(io::location_view location, memory_resource* resource) {
  const std::string_view text = location.string_view();
  core::string result{allocator<char>{resource}};
  result.assign(text.begin(), text.end());
  return result;
}

class buffered_source_output_stream final : public io::output_stream {
public:
  buffered_source_output_stream(source& target, io::location_view location,
                                memory_resource* resource)
      : target_(&target), location_(make_location(location, resource)),
        buffer_(io::make_byte_buffer(resource)) {}

  [[nodiscard]] io::size_result write(std::span<const std::byte> buffer) override {
    if (closed_) {
      return fail<std::size_t>({error_domain::resources, errc::invalid_argument});
    }

    buffer_.insert(buffer_.end(), buffer.begin(), buffer.end());
    return ok(buffer.size());
  }

  [[nodiscard]] error close() override {
    if (closed_) {
      return {};
    }

    error status = target_->write(io::location_view{location_}, buffer_);
    if (status) {
      return status;
    }

    closed_ = true;
    return {};
  }

private:
  source* target_ = nullptr;
  core::string location_;
  io::byte_buffer buffer_;
  bool closed_ = false;
};

} // namespace

source::~source() = default;

result<captured_source> source::capture() const {
  return fail<captured_source>({error_domain::resources, errc::unsupported});
}

io::input_stream_result source::open_read(io::location_view location, memory_resource* resource) {
  io::byte_buffer_result bytes = read(location, resource);
  if (bytes.failed()) {
    return fail<io::input_stream_ptr>(bytes.error());
  }

  return ok(io::make_input_stream(std::move(bytes.value), resource));
}

error source::write(io::location_view, std::span<const std::byte>) {
  return {error_domain::resources, errc::unsupported};
}

io::output_stream_result source::open_write(io::location_view location, memory_resource* resource) {
  if (location.empty()) {
    return fail<io::output_stream_ptr>({error_domain::resources, errc::invalid_argument});
  }

  memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
  auto* stream = memory::make<buffered_source_output_stream>(resolved, *this, location, resolved);
  io::output_stream_ptr result{stream};
  result.get_deleter().reset(resolved, [](void* user_data, void* stored) noexcept {
    memory::destroy(static_cast<buffered_source_output_stream*>(stored),
                    static_cast<memory_resource*>(user_data));
  });
  return ok(std::move(result));
}

file_source::file_source(io::file_system& file_system) noexcept : file_system_(&file_system) {}

file_source::file_source(std::shared_ptr<io::file_system> file_system,
                         std::shared_ptr<void> lifetime) {
  struct held_file_system {
    std::shared_ptr<void> lifetime;
    std::shared_ptr<io::file_system> target;
  };
  if (file_system) {
    auto held = std::make_shared<held_file_system>(
        held_file_system{std::move(lifetime), std::move(file_system)});
    owner_ = std::shared_ptr<io::file_system>{held, held->target.get()};
    file_system_ = owner_.get();
  }
}

namespace {
// 仅兼容来源需要该单向适配；新 VFS 不再包装为旧 file_system。
class legacy_backend final : public io::backend {
public:
  legacy_backend(std::shared_ptr<io::file_system> files, std::uint64_t source_id)
      : files_(std::move(files)), source_id_(source_id) {}
  io::io_error open(io::path location, io::open_options options, memory_resource* resource,
                    io::io_completion<std::shared_ptr<io::async_file>> completion) override {
    return files_->start_open_options({files_, std::move(location), source_id_}, options, resource,
                                      std::move(completion));
  }

private:
  std::shared_ptr<io::file_system> files_;
  std::uint64_t source_id_;
};
}
result<captured_source> file_source::capture() const {
  if (!file_system_)
    return fail<captured_source>({error_domain::resources, errc::invalid_argument});
  io::detail::snapshot_context context;
  auto frozen = context.capture(*file_system_, owner_);
  if (!frozen)
    return fail<captured_source>(frozen.error());
  return {
      {},
      [root = std::move(frozen.value)](io::location_view location) -> result<io::backend_location> {
        auto resolved = io::detail::resolve_snapshot(root, location);
        if (!resolved)
          return {resolved.error(), {}};
        return {
            {},
            {std::make_shared<legacy_backend>(std::move(resolved->backend), resolved->source_id),
             std::move(resolved->path), resolved->source_id}};
      }};
}

io::file_system& file_source::file_system() const noexcept { return *file_system_; }

bool file_source::exists(io::location_view location) const {
  return file_system_ != nullptr && file_system_->exists(location);
}

io::byte_buffer_result file_source::read(io::location_view location, memory_resource* resource) {
  if (file_system_ == nullptr) {
    return fail<io::byte_buffer>({error_domain::resources, errc::backend_failure});
  }

  return file_system_->read_all(location, resource);
}

io::input_stream_result file_source::open_read(io::location_view location,
                                               memory_resource* resource) {
  if (file_system_ == nullptr) {
    return fail<io::input_stream_ptr>({error_domain::resources, errc::backend_failure});
  }

  return file_system_->open_read(location, resource);
}

error file_source::write(io::location_view location, std::span<const std::byte> bytes) {
  if (file_system_ == nullptr) {
    return {error_domain::resources, errc::backend_failure};
  }

  return file_system_->write_all(location, bytes);
}

io::output_stream_result file_source::open_write(io::location_view location,
                                                 memory_resource* resource) {
  if (file_system_ == nullptr) {
    return fail<io::output_stream_ptr>({error_domain::resources, errc::backend_failure});
  }

  return file_system_->open_write(location, resource);
}

provider_snapshot::provider_snapshot(memory_resource* resource)
    : locations_(typename location_map::allocator_type{resource}) {}

result<io::backend_location> provider_snapshot::resolve(resource_guid guid) const {
  const auto found = locations_.find(guid);
  if (found == locations_.end())
    return fail<io::backend_location>({error_domain::resources, errc::not_found});
  const auto& entry = found->second;
  if (entry.status)
    return fail<io::backend_location>(entry.status);
  return ok(entry.location);
}

std::shared_ptr<const provider_snapshot> provider::snapshot() const {
  auto result =
      std::allocate_shared<provider_snapshot>(allocator<provider_snapshot>{resource_}, resource_);
  using frozen_map = core::hash_map<const resources::source*, caors::result<captured_source>>;
  frozen_map frozen{typename frozen_map::allocator_type{resource_}};
  for (const auto& [guid, binding] : locations_) {
    auto found = frozen.find(binding.source);
    if (found == frozen.end()) {
      auto root = binding.source->capture();
      if (root && !root.value)
        root.status = {error_domain::resources, errc::backend_failure};
      found = frozen.emplace(binding.source, std::move(root)).first;
    }
    // 仅在本次捕获内共享，下一次仍重新冻结，避免错过外部或嵌套路由变化。
    const auto& root = found->second;
    provider_snapshot::entry entry{{}, root.error()};
    if (root) {
      auto resolved = root.value(io::location_view{binding.location});
      if (resolved && !resolved->backend)
        resolved.status = {error_domain::resources, errc::backend_failure};
      entry.status = resolved.error();
      if (resolved) {
        entry.location = std::move(resolved.value);
        // 路径归入 provider 分配器，快照结果不借用解析函数的临时字符串。
        entry.location.path = io::path{entry.location.path.view(), resource_};
      }
    }
    result->locations_.emplace(guid, std::move(entry));
  }
  return result;
}

provider::provider(memory_resource* resource)
    : resource_(resource != nullptr ? resource : get_default_memory_resource()),
      locations_(typename location_map::allocator_type{resource_}) {}

memory_resource* provider::resource() const noexcept { return resource_; }

error provider::bind(resource_guid guid, resources::source& source, io::location_view location) {
  if (!guid || location.empty()) {
    return {error_domain::resources, errc::invalid_argument};
  }
  if (contains(guid)) {
    return {error_domain::resources, errc::already_exists};
  }

  locations_.emplace(guid, entry{&source, make_location(location, resource_)});
  return {};
}

error provider::rebind(resource_guid guid, resources::source& source, io::location_view location) {
  if (!guid || location.empty()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  entry updated{&source, make_location(location, resource_)};
  const auto position = locations_.find(guid);
  if (position == locations_.end()) {
    locations_.emplace(guid, std::move(updated));
    return {};
  }

  position->second = std::move(updated);
  return {};
}

bool provider::unbind(resource_guid guid) { return locations_.erase(guid) != 0; }

void provider::clear() noexcept { locations_.clear(); }

std::size_t provider::size() const noexcept { return locations_.size(); }

bool provider::empty() const noexcept { return locations_.empty(); }

bool provider::contains(resource_guid guid) const {
  return guid && locations_.find(guid) != locations_.end();
}

source_location provider::find(resource_guid guid) const {
  const auto position = locations_.find(guid);
  if (position == locations_.end()) {
    return {};
  }

  return {position->second.source, io::location_view{position->second.location}};
}

bool provider::exists(resource_guid guid) const {
  const source_location location = find(guid);
  return location && location.source->exists(location.location);
}

io::byte_buffer_result provider::read(resource_guid guid, memory_resource* resource) const {
  if (!guid) {
    return fail<io::byte_buffer>({error_domain::resources, errc::invalid_argument});
  }

  const source_location location = find(guid);
  if (!location) {
    return fail<io::byte_buffer>({error_domain::resources, errc::not_found});
  }

  return location.source->read(location.location, resource);
}

io::input_stream_result provider::open_read(resource_guid guid, memory_resource* resource) const {
  if (!guid) {
    return fail<io::input_stream_ptr>({error_domain::resources, errc::invalid_argument});
  }

  const source_location location = find(guid);
  if (!location) {
    return fail<io::input_stream_ptr>({error_domain::resources, errc::not_found});
  }

  return location.source->open_read(location.location, resource);
}

error provider::write(resource_guid guid, std::span<const std::byte> bytes) const {
  if (!guid) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const source_location location = find(guid);
  if (!location) {
    return {error_domain::resources, errc::not_found};
  }

  return location.source->write(location.location, bytes);
}

io::output_stream_result provider::open_write(resource_guid guid, memory_resource* resource) const {
  if (!guid) {
    return fail<io::output_stream_ptr>({error_domain::resources, errc::invalid_argument});
  }

  const source_location location = find(guid);
  if (!location) {
    return fail<io::output_stream_ptr>({error_domain::resources, errc::not_found});
  }

  return location.source->open_write(location.location, resource);
}

} // namespace caors::resources
