// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_PLATFORM_SHARED_LIBRARY_H_
#define CAORS_PLATFORM_SHARED_LIBRARY_H_

#include "caors/export.h"
#include "caors/result.h"

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::platform {

/** @brief 动态库符号的可见范围。 */
enum class library_load_mode : std::uint8_t {
  /** @brief 符号只用于当前动态库及其依赖。 */
  local,
  /** @brief 在支持的平台上将符号加入进程级全局查找范围。 */
  global,
};

/**
 * @brief 跨平台动态库所有权封装。
 *
 * `shared_library` 独占一个原生动态库引用。由该动态库取得的函数指针和对象不得超过本对象的
 * 生命周期。该类型可移动、不可复制。
 */
class CAORS_API shared_library final {
public:
  shared_library() noexcept = default;
  shared_library(const shared_library&) = delete;
  shared_library& operator=(const shared_library&) = delete;
  shared_library(shared_library&& other) noexcept;
  shared_library& operator=(shared_library&& other) noexcept;
  ~shared_library();

  /**
   * @brief 打开动态库。
   *
   * @param path 动态库文件路径。Windows 接受原生宽字符路径。
   * @param mode POSIX 符号可见范围；Windows 当前忽略该选项。
   * @return 成功时返回持有动态库的对象；失败时返回 platform 错误。
   */
  [[nodiscard]] static result<shared_library>
  open(const std::filesystem::path& path, library_load_mode mode = library_load_mode::local);

  /** @brief 判断当前对象是否持有动态库。 */
  [[nodiscard]] bool loaded() const noexcept { return handle_ != nullptr; }
  [[nodiscard]] explicit operator bool() const noexcept { return loaded(); }

  /** @brief 返回打开时使用的路径。 */
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

  /**
   * @brief 查询导出符号。
   *
   * 返回指针只在动态库保持加载期间有效。找不到符号或参数为空时返回空指针。
   */
  [[nodiscard]] void* find_symbol(std::string_view name) const;

  /** @brief 按函数指针类型查询导出函数。 */
  template <typename Function>
    requires(std::is_pointer_v<Function> && std::is_function_v<std::remove_pointer_t<Function>>)
  [[nodiscard]] Function find_function(std::string_view name) const {
    return reinterpret_cast<Function>(find_symbol(name));
  }

  /**
   * @brief 释放当前动态库引用。
   *
   * 未加载时幂等成功。成功后所有此前取得的符号都立即失效。
   */
  [[nodiscard]] error close() noexcept;

private:
  explicit shared_library(void* handle, std::filesystem::path path) noexcept
      : handle_(handle), path_(std::move(path)) {}

  void* handle_ = nullptr;
  std::filesystem::path path_{};
};

} // namespace caors::platform

#endif
