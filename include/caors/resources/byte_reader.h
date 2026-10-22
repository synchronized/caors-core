// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_BYTE_READER_H_
#define CAORS_RESOURCES_BYTE_READER_H_

#include "caors/io/async_file.h"
#include "caors/io/route_snapshot.h"

namespace caors::resources {
/** @brief 拥有式读取来源及单次预算；worker 必须支持后端要求的执行方式。 */
struct read_context {
  /** @brief 拥有后端和路径的来源；宿主实现可按自身协议解释或忽略。 */
  io::resolved_file source;
  /** @brief 内部打开、读取及关闭通知使用的调度入口。 */
  tasks::executor worker;
  /** @brief 单次结果预算；后端必须在分配前检查，不代表全局在途预算。 */
  std::uint64_t max_bytes = 64 * 1024 * 1024;
  /** @brief 借用的字节分配器，必须覆盖真实操作和结果寿命。 */
  memory_resource* resource = nullptr;
};
/** @brief 复用 IO 的唯一完成凭证、取消和拥有式字节结果。 */
using read_completion = io::io_completion<io::byte_buffer>;
/**
 * @brief 整体字节读取协议；返回值表示接受或拒绝，实际结果通过 completion 交付。
 * @note 接受后必须完成或销毁凭证；拒绝不得保留凭证。允许立即完成，不允许阻塞等待宿主循环。
 * 实现及 allocator 必须覆盖真实操作；宿主后端可直接实现此接口，无须创建 VFS 后端。
 */
class CAORS_API byte_reader {
public:
  virtual ~byte_reader();
  [[nodiscard]] virtual io::io_error read(read_context context, read_completion completion) = 0;
};
/** @brief VFS 打开策略；不在 unsupported 后隐式回退同步读取。 */
enum class file_read_mode { native_async, worker_adapter };
/**
 * @brief 将 VFS 打开、限量读取和关闭组合为一次读取；所有内部通知经 context.worker。
 * @note worker_adapter 使用同步随机读后端，须配置可阻塞 worker；native_async 不作同步回退。
 * 取消在阶段之间协作观察，不强制中断在途读取；正常通知路径交付前显式关闭，投递丢弃时由 RAII 兜底。
 * 当前 IO 请求结果只读，交付前会复制字节，峰值结果内存最多约为两份 max_bytes，另加后端开销。
 */
class CAORS_API file_byte_reader final : public byte_reader {
public:
  explicit file_byte_reader(file_read_mode mode = file_read_mode::native_async) : mode_(mode) {}
  [[nodiscard]] io::io_error read(read_context context, read_completion completion) override;

private:
  file_read_mode mode_;
};
/** @brief 转换为资源管线错误；IO 原始细节仍由字节请求结果保留。 */
[[nodiscard]] inline error read_error(io::io_error status) noexcept {
  if (!status)
    return {};
  errc code = errc::backend_failure;
  switch (status.value) {
  case io::io_errc::cancelled:
    code = errc::cancelled;
    break;
  case io::io_errc::not_found:
    code = errc::not_found;
    break;
  case io::io_errc::unsupported:
    code = errc::unsupported;
    break;
  case io::io_errc::invalid_argument:
    code = errc::invalid_argument;
    break;
  case io::io_errc::out_of_memory:
    code = errc::out_of_memory;
    break;
  default:
    break;
  }
  return {error_domain::io, code};
}
/** @brief 发起统一读取；reader 借用至真实完成，回调只在 completion_executor 执行。 */
[[nodiscard]] inline io::io_result<io::io_request<io::byte_buffer>>
request_bytes(byte_reader& reader, read_context context, tasks::executor completion_executor,
              std::function<void(const io::io_result<io::byte_buffer>&)> callback = {}) {
  return io::start_io<io::byte_buffer>(
      std::move(completion_executor), std::move(callback),
      [&reader, context = std::move(context)](read_completion receipt) mutable {
        return reader.read(std::move(context), std::move(receipt));
      });
}
} // namespace caors::resources
#endif
