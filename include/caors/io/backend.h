// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_IO_BACKEND_H_
#define CAORS_IO_BACKEND_H_
#include "caors/io/async_file.h"
#include "caors/io/options.h"
namespace caors::io {
class memory_file_system;
class file_system;
/**
 * @brief 独立文件后端协议，只处理后端相对路径，不管理挂载与用户调度。
 * @note 接受后必须完成或销毁凭证；拒绝不得保留凭证。实现可绑定 worker 或宿主 loop。
 */
class CAORS_API backend {
public:
  virtual ~backend();
  /** @brief 打开只读已有文件；path 拥有字符串，resource 须覆盖实际操作与结果。 */
  [[nodiscard]] virtual io_error open(path path, open_options options, memory_resource* resource,
                                      io_completion<std::shared_ptr<async_file>> completion) = 0;
};
/** @brief 本地同步随机读取适配；构造绑定可阻塞 worker，不隐式选择执行环境。 */
[[nodiscard]] CAORS_API std::shared_ptr<backend>
make_native_backend(path_view root, tasks::executor worker, memory_resource* resource = nullptr);
/** @brief 内存存储后端；store 是配置数据操作，不执行外部 I/O，不允许与读取无协调并发。 */
class CAORS_API memory_backend final : public backend {
public:
  explicit memory_backend(memory_resource* resource = nullptr);
  [[nodiscard]] error store(path_view location, std::span<const std::byte> bytes);
  [[nodiscard]] error store(path_view location, std::string_view text) {
    return store(location, {reinterpret_cast<const std::byte*>(text.data()), text.size()});
  }
  [[nodiscard]] io_error open(path location, open_options options, memory_resource* resource,
                              io_completion<std::shared_ptr<async_file>> completion) override;

private:
  std::shared_ptr<memory_file_system> files_;
};
/** @brief 创建可挂载的内存后端，配置数据使用 store。 */
[[nodiscard]] CAORS_API std::shared_ptr<memory_backend>
make_memory_backend(memory_resource* resource = nullptr);
} // namespace caors::io
#endif
