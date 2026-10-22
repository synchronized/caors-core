// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_MANIFEST_FILE_SYSTEM_H_
#define CAORS_RESOURCES_MANIFEST_FILE_SYSTEM_H_

#include "caors/io/file_system.h"
#include "caors/resources/manifest.h"

#include <span>

namespace caors::resources {

/**
 * @brief 使用资源 manifest 补充目录枚举的文件系统 overlay。
 *
 * `manifest_file_system` 不拥有 manifest 或底层文件系统。目录结构由 manifest 中的 location 推导；
 * 字节读取和写入仍转发给底层 `io::file_system`。它适合
 * HTTP、CDN、资源包索引等不能直接列目录的后端。
 */
class CAORS_API manifest_file_system final : public io::file_system {
public:
  using io::file_system::write_all;

  explicit manifest_file_system(const manifest& entries, memory_resource* resource = nullptr);
  manifest_file_system(const manifest& entries, io::file_system& target,
                       memory_resource* resource = nullptr);

  /** @brief 返回内部临时字符串使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept;

  /** @brief 返回当前清单。 */
  [[nodiscard]] const manifest& entries() const noexcept;

  /** @brief 返回底层文件系统；没有底层时返回 nullptr。 */
  [[nodiscard]] io::file_system* target() const noexcept;

  [[nodiscard]] bool exists(io::path_view path) const override;
  [[nodiscard]] io::file_system_capabilities capabilities() const noexcept override;
  [[nodiscard]] io::file_status_result status(io::path_view path) const override;
  [[nodiscard]] io::directory_entry_list_result list(io::path_view directory,
                                                     memory_resource* resource = nullptr) override;
  [[nodiscard]] io::byte_buffer_result read_all(io::path_view path,
                                                memory_resource* resource = nullptr) override;
  [[nodiscard]] io::input_stream_result open_read(io::path_view path,
                                                  memory_resource* resource = nullptr) override;
  [[nodiscard]] io::output_stream_result open_write(io::path_view path,
                                                    memory_resource* resource = nullptr) override;
  [[nodiscard]] error write_all(io::path_view path, std::span<const std::byte> bytes) override;

private:
  [[nodiscard]] bool manifest_contains_file(io::path_view path) const;
  [[nodiscard]] bool manifest_contains_directory(io::path_view path) const;

  const manifest* entries_ = nullptr;
  io::file_system* target_ = nullptr;
  memory_resource* resource_ = nullptr;
};

} // namespace caors::resources

#endif
