// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_MEMORY_FILE_SYSTEM_H_
#define CAORS_IO_MEMORY_FILE_SYSTEM_H_

#include "caors/io/file_system.h"

#include <cstddef>
#include <functional>
#include <string_view>

namespace caors::io {

/**
 * @brief 基于内存哈希表的同步文件系统实现。
 *
 * `memory_file_system` 使用规范化虚拟路径保存字节内容，适合测试、调试热更新缓存和后续资源包索引的
 * 内存镜像。它不会模拟目录、权限、时间戳或宿主平台路径语义。
 */
class CAORS_API memory_file_system final : public file_system {
public:
  using file_system::write_all;

  explicit memory_file_system(memory_resource* resource = nullptr);

  /** @brief 返回内部索引和文件内容使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept;

  /** @brief 返回已保存文件数量。 */
  [[nodiscard]] std::size_t file_count() const noexcept;
  [[nodiscard]] bool empty() const noexcept;

  /** @brief 删除指定路径内容。 */
  [[nodiscard]] bool erase(path_view path);

  /** @brief 清空全部内容。 */
  void clear() noexcept;

  [[nodiscard]] bool exists(path_view path) const override;
  [[nodiscard]] byte_buffer_result read_all(path_view path,
                                            memory_resource* resource = nullptr) override;
  [[nodiscard]] error write_all(path_view path, std::span<const std::byte> bytes) override;

private:
  struct path_hash {
    [[nodiscard]] std::size_t operator()(const core::string& value) const noexcept {
      return std::hash<std::string_view>{}(std::string_view{value.data(), value.size()});
    }
  };

  struct path_equal {
    [[nodiscard]] bool operator()(const core::string& lhs, const core::string& rhs) const noexcept {
      return lhs == rhs;
    }
  };

  using file_map = core::hash_map<core::string, byte_buffer, path_hash, path_equal>;

  [[nodiscard]] core::string key_for(path_view path) const;

  memory_resource* resource_ = nullptr;
  file_map files_;
};

} // namespace caors::io

#endif
