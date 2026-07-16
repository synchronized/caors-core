// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_SOURCE_H_
#define CAORS_RESOURCES_SOURCE_H_

#include "caors/core/containers.h"
#include "caors/io/file_system.h"
#include "caors/resources/id.h"

#include <cstddef>
#include <span>
#include <string_view>

namespace caors::resources {

/**
 * @brief 资源字节来源接口。
 *
 * `source` 负责按字符串位置访问字节，不负责解析资源 GUID、缓存资源对象或决定异步策略。默认只读；
 * 支持写入的实现应覆盖 `write()`。
 */
class CAORS_API source {
public:
  source() = default;
  virtual ~source();

  source(const source&) = delete;
  source& operator=(const source&) = delete;
  source(source&&) = delete;
  source& operator=(source&&) = delete;

  /** @brief 判断指定位置是否存在。 */
  [[nodiscard]] virtual bool exists(io::path_view location) const = 0;

  /** @brief 读取指定位置的完整字节内容。 */
  [[nodiscard]] virtual io::byte_buffer_result read(io::path_view location,
                                                    memory_resource* resource = nullptr) = 0;

  /** @brief 写入指定位置的完整字节内容；默认返回 `errc::unsupported`。 */
  [[nodiscard]] virtual error write(io::path_view location, std::span<const std::byte> bytes);

  /** @brief 写入指定位置的字符串内容。 */
  [[nodiscard]] error write(io::path_view location, std::string_view text) {
    return write(location, std::span<const std::byte>{
                               reinterpret_cast<const std::byte*>(text.data()), text.size()});
  }
};

/**
 * @brief 把 `io::file_system` 适配为资源字节来源。
 *
 * `file_source` 不拥有底层文件系统，调用方需要保证文件系统在 source 使用期间保持有效。
 */
class CAORS_API file_source final : public source {
public:
  using source::write;

  explicit file_source(io::file_system& file_system) noexcept;

  /** @brief 返回底层文件系统。 */
  [[nodiscard]] io::file_system& file_system() const noexcept;

  [[nodiscard]] bool exists(io::path_view location) const override;
  [[nodiscard]] io::byte_buffer_result read(io::path_view location,
                                            memory_resource* resource = nullptr) override;
  [[nodiscard]] error write(io::path_view location, std::span<const std::byte> bytes) override;

private:
  io::file_system* file_system_ = nullptr;
};

/**
 * @brief provider 查询到的资源位置视图。
 *
 * 视图不拥有 location 字符串，provider 修改或销毁后该视图失效。
 */
struct source_location {
  resources::source* source = nullptr;
  io::path_view location{};

  [[nodiscard]] explicit operator bool() const noexcept {
    return source != nullptr && !location.empty();
  }
};

/**
 * @brief `resource_guid` 到资源字节来源和位置的映射。
 *
 * `provider` 不拥有 source，只保存 GUID 到 `(source, location)` 的索引。它适合作为资源数据库、
 * manifest 或调试 fileserver manifest 的第一阶段运行期表示。
 */
class CAORS_API provider final {
public:
  explicit provider(memory_resource* resource = nullptr);

  provider(const provider&) = delete;
  provider& operator=(const provider&) = delete;
  provider(provider&&) = delete;
  provider& operator=(provider&&) = delete;

  /** @brief 返回内部索引使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept;

  /** @brief 绑定 GUID 到指定来源和位置。 */
  [[nodiscard]] error bind(resource_guid guid, resources::source& source, io::path_view location);

  /** @brief 绑定或更新 GUID 到指定来源和位置。 */
  [[nodiscard]] error rebind(resource_guid guid, resources::source& source, io::path_view location);

  /** @brief 解绑 GUID。 */
  [[nodiscard]] bool unbind(resource_guid guid);

  /** @brief 清空全部绑定。 */
  void clear() noexcept;

  /** @brief 查询绑定数量。 */
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept;

  /** @brief 判断 GUID 是否存在绑定。 */
  [[nodiscard]] bool contains(resource_guid guid) const;

  /** @brief 查询 GUID 对应的来源和位置，未命中时返回空视图。 */
  [[nodiscard]] source_location find(resource_guid guid) const;

  /** @brief 判断 GUID 对应的位置是否存在。 */
  [[nodiscard]] bool exists(resource_guid guid) const;

  /** @brief 读取 GUID 对应位置的完整字节内容。 */
  [[nodiscard]] io::byte_buffer_result read(resource_guid guid,
                                            memory_resource* resource = nullptr) const;

  /** @brief 写入 GUID 对应位置的完整字节内容。 */
  [[nodiscard]] error write(resource_guid guid, std::span<const std::byte> bytes) const;

  /** @brief 写入 GUID 对应位置的字符串内容。 */
  [[nodiscard]] error write(resource_guid guid, std::string_view text) const {
    return write(guid, std::span<const std::byte>{reinterpret_cast<const std::byte*>(text.data()),
                                                  text.size()});
  }

private:
  struct entry {
    resources::source* source = nullptr;
    core::string location;
  };

  using location_map = core::hash_map<resource_guid, entry, resource_guid_hash>;

  memory_resource* resource_ = nullptr;
  location_map locations_;
};

} // namespace caors::resources

#endif
