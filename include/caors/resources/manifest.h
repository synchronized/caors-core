// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_MANIFEST_H_
#define CAORS_RESOURCES_MANIFEST_H_

#include "caors/core/containers.h"
#include "caors/core/string_name.h"
#include "caors/io/uri.h"
#include "caors/resources/id.h"
#include "caors/resources/source.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace caors::resources {

/**
 * @brief 资源清单条目。
 *
 * `manifest_entry` 保存稳定资源 GUID 到虚拟资源位置的映射，并可附带类型名、内容 hash 和版本号。
 * 它不拥有资源对象，也不负责读取或解析资源字节。
 */
struct manifest_entry {
  resource_guid guid{};
  io::path location{};
  core::string_name type{};
  core::string hash;
  std::uint64_t version = 0;

  explicit manifest_entry(memory_resource* resource = nullptr)
      : location(resource), hash(allocator<char>{resource}) {}

  manifest_entry(resource_guid resource_guid, io::path_view resource_location,
                 core::string_name resource_type = {}, std::string_view resource_hash = {},
                 std::uint64_t resource_version = 0, memory_resource* resource = nullptr)
      : guid(resource_guid), location(io::normalize_location(resource_location, resource)),
        type(resource_type), hash(allocator<char>{resource}), version(resource_version) {
    hash.assign(resource_hash.begin(), resource_hash.end());
  }

  /** @brief 判断条目是否具备可绑定到 provider 的最小信息。 */
  [[nodiscard]] bool valid() const noexcept { return guid && !location.empty(); }
};

/**
 * @brief 资源 GUID 到资源位置和轻量元数据的清单。
 *
 * `manifest` 适合承载本地资源数据库、资源包索引或调试 fileserver 下发的清单。它只保存清单数据，
 * 真正的字节读取仍由 `resources::source` / `resources::provider` 完成。
 */
class manifest final {
public:
  explicit manifest(memory_resource* resource = nullptr)
      : resource_(resource != nullptr ? resource : get_default_memory_resource()),
        entries_(typename entry_map::allocator_type{resource_}) {}

  manifest(const manifest&) = delete;
  manifest& operator=(const manifest&) = delete;
  manifest(manifest&&) = delete;
  manifest& operator=(manifest&&) = delete;

  /** @brief 返回内部索引和字符串使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept { return resource_; }

  /** @brief 查询清单条目数量。 */
  [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
  [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }

  /** @brief 清空全部清单条目。 */
  void clear() noexcept { entries_.clear(); }

  /** @brief 新增资源清单条目。location 会按资源位置规则规范化。 */
  [[nodiscard]] error bind(resource_guid guid, io::path_view location, core::string_name type = {},
                           std::string_view hash = {}, std::uint64_t version = 0) {
    if (contains(guid)) {
      return {error_domain::resources, errc::already_exists};
    }

    return set(guid, location, type, hash, version);
  }

  /** @brief 新增资源清单条目，并把输入条目内容复制到当前 manifest 的内存资源中。 */
  [[nodiscard]] error bind(const manifest_entry& entry) {
    return bind(entry.guid, entry.location, entry.type,
                std::string_view{entry.hash.data(), entry.hash.size()}, entry.version);
  }

  /** @brief 新增或替换资源清单条目。location 会按资源位置规则规范化。 */
  [[nodiscard]] error set(resource_guid guid, io::path_view location, core::string_name type = {},
                          std::string_view hash = {}, std::uint64_t version = 0) {
    if (!guid || location.empty()) {
      return {error_domain::resources, errc::invalid_argument};
    }

    manifest_entry entry{resource_};
    entry.guid = guid;
    entry.location = io::normalize_location(location, resource_);
    if (entry.location.empty()) {
      return {error_domain::resources, errc::invalid_argument};
    }
    entry.type = type;
    entry.hash.assign(hash.begin(), hash.end());
    entry.version = version;

    const auto position = entries_.find(guid);
    if (position == entries_.end()) {
      entries_.emplace(guid, std::move(entry));
      return {};
    }

    position->second = std::move(entry);
    return {};
  }

  /** @brief 新增或替换资源清单条目，并把输入条目内容复制到当前 manifest 的内存资源中。 */
  [[nodiscard]] error set(const manifest_entry& entry) {
    return set(entry.guid, entry.location, entry.type,
               std::string_view{entry.hash.data(), entry.hash.size()}, entry.version);
  }

  /** @brief 删除指定 GUID 的清单条目。 */
  [[nodiscard]] bool unbind(resource_guid guid) { return entries_.erase(guid) != 0; }

  /** @brief 判断 GUID 是否存在清单条目。 */
  [[nodiscard]] bool contains(resource_guid guid) const {
    return guid && entries_.find(guid) != entries_.end();
  }

  /** @brief 查询 GUID 对应清单条目，未命中时返回 nullptr。 */
  [[nodiscard]] const manifest_entry* find(resource_guid guid) const {
    const auto position = entries_.find(guid);
    return position != entries_.end() ? &position->second : nullptr;
  }

  /** @brief 遍历全部清单条目。 */
  template <typename Visitor> void for_each(Visitor&& visitor) const {
    for (const auto& pair : entries_) {
      visitor(pair.second);
    }
  }

private:
  using entry_map = core::hash_map<resource_guid, manifest_entry, resource_guid_hash>;

  memory_resource* resource_ = nullptr;
  entry_map entries_;
};

/**
 * @brief 把清单中的 GUID/location 批量绑定到 provider。
 *
 * `bind_manifest()` 不拥有 source，也不回滚已经成功绑定的条目；调用方需要在失败时按自身策略处理
 * provider 中已添加的绑定。
 */
[[nodiscard]] inline error bind_manifest(provider& target, source& source,
                                         const manifest& entries) {
  error status{};
  entries.for_each([&](const manifest_entry& entry) {
    if (status.failed()) {
      return;
    }

    status = target.bind(entry.guid, source, entry.location);
  });

  return status;
}

/**
 * @brief 把清单中的 GUID/location 批量同步到 provider。
 *
 * `sync_manifest()` 会对每个条目调用 `provider::rebind()`：已存在的 GUID 会更新位置或 source，
 * 不存在的 GUID 会新增。它不会删除 provider 中清单未包含的旧绑定。
 */
[[nodiscard]] inline error sync_manifest(provider& target, source& source,
                                         const manifest& entries) {
  error status{};
  entries.for_each([&](const manifest_entry& entry) {
    if (status.failed()) {
      return;
    }

    status = target.rebind(entry.guid, source, entry.location);
  });

  return status;
}

/**
 * @brief 使用完整清单替换 provider 中的全部 GUID/location 绑定。
 *
 * `replace_manifest()` 会先清空 provider，再把清单条目重新绑定进去。它适合资源数据库或调试
 * fileserver 下发完整清单的场景；如果绑定失败，函数不会恢复清空前的 provider 状态。
 */
[[nodiscard]] inline error replace_manifest(provider& target, source& source,
                                            const manifest& entries) {
  target.clear();
  return bind_manifest(target, source, entries);
}

} // namespace caors::resources

namespace caors {

using resource_manifest = resources::manifest;

} // namespace caors

#endif
