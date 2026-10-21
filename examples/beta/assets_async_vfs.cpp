// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/assets.h"
#include "caors/io.h"
#include "caors/tasks.h"

#include <iostream>

namespace {
class text_asset final : public caors::resource {
public:
  using caors::resource::resource;
  std::string text;
};

// 示例只解析 UTF-8 文本；真实格式校验、摘要复验和全局在途预算由宿主管理。
class text_loader final : public caors::resources::async_loader<text_asset> {
  struct operation {
    caors::resources::async_load_context context;
    caors::resources::load_completion<text_asset> completion;
    caors::io::io_request<std::shared_ptr<caors::io::async_file>> opening;
    caors::io::io_request<caors::io::byte_buffer> reading;
    caors::io::io_request<caors::io::close_result> closing;
    std::shared_ptr<caors::io::async_file> file;
    caors::resources::load_result<text_asset> value;
    using stop_callback = std::stop_callback<std::function<void()>>;
    std::unique_ptr<stop_callback> stop;
  };
  static void finish(const std::shared_ptr<operation>& op, caors::error error) {
    auto completion = std::move(op->completion);
    if (error && !op->value.status)
      op->value.status = error;
    (void)completion.complete(std::move(op->value));
  }
  static void close(const std::shared_ptr<operation>& op) {
    auto result = caors::io::request_close(op->file, op->context.worker, [op](const auto& closed) {
      finish(op, closed ? caors::error{}
                        : caors::error{caors::error_domain::io, caors::errc::backend_failure});
    });
    if (!result)
      finish(op, {caors::error_domain::io, caors::errc::backend_failure});
    else
      op->closing = std::move(result.value);
  }
  static void read(const std::shared_ptr<operation>& op) {
    auto result = caors::io::read_all_bounded(
        op->file, 1024, op->context.worker,
        [op](const auto& bytes) {
          if (!bytes || op->context.stop.stop_requested()) {
            op->value.status = {caors::error_domain::io,
                                op->context.stop.stop_requested() ||
                                        bytes.error().value == caors::io::io_errc::cancelled
                                    ? caors::errc::cancelled
                                    : caors::errc::backend_failure};
          } else {
            auto value =
                caors::memory::make_unique<text_asset>(op->context.resource, op->context.guid);
            value->text.assign(reinterpret_cast<const char*>(bytes.value.data()),
                               bytes.value.size());
            op->value.value = std::move(value);
          }
          close(op);
        },
        op->context.resource);
    if (!result) {
      op->value.status = {caors::error_domain::io, caors::errc::backend_failure};
      close(op);
    } else
      op->reading = std::move(result.value);
  }

public:
  explicit text_loader(bool use_worker) : use_worker_(use_worker) {}
  caors::error start(caors::resources::async_load_context context,
                     caors::resources::load_completion<text_asset> completion) override {
    auto sources = context.snapshot_as<caors::resources::provider_snapshot>();
    if (!sources)
      return {caors::error_domain::resources, caors::errc::unsupported};
    auto resolved = sources->resolve(context.guid);
    if (!resolved)
      return resolved.error();
    auto op = std::make_shared<operation>();
    op->context = std::move(context);
    op->completion = std::move(completion);
    std::weak_ptr<operation> weak = op;
    // 将取消转回 worker；不在请求取消线程操作宿主 I/O 句柄。
    op->stop = std::make_unique<operation::stop_callback>(
        op->context.stop, [weak, worker = op->context.worker] {
          (void)worker.post(caors::tasks::task{[weak] {
            if (auto op = weak.lock()) {
              op->opening.cancel();
              op->reading.cancel();
            }
          }});
        });
    auto on_open = [op](const auto& file) {
      if (!file) {
        finish(op, {caors::error_domain::io, caors::errc::backend_failure});
        return;
      }
      op->file = file.value;
      read(op);
    };
    // 策略由构造配置明确选择，不在 unsupported 之后隐式回退。
    auto opened =
        use_worker_
            ? caors::io::request_open_on_worker(std::move(resolved.value), op->context.worker,
                                                op->context.worker, on_open, op->context.resource)
            : caors::io::request_open(std::move(resolved.value), op->context.worker, on_open,
                                      op->context.resource);
    if (!opened)
      finish(op, {caors::error_domain::io, caors::errc::unsupported});
    else
      op->opening = std::move(opened.value);
    return {};
  }

private:
  bool use_worker_;
};

int run_example(bool use_worker) {
  caors::tasks::execution_context worker;
  caors::tasks::execution_context completion;
  caors::resources::cache<text_asset> cache;
  text_loader loader{use_worker};
  caors::assets::asset_manager assets{{worker.get_executor(), completion.get_executor()}};
  auto memory = std::make_shared<caors::io::memory_file_system>();
  if (memory->write_all("hello", std::string_view{"hello VFS"}))
    return 1;
  auto router = std::make_shared<caors::io::file_system_resolver>();
  if (router->mount("caors://assets", memory))
    return 2;
  caors::resources::file_source source{router};
  const caors::resource_guid guid{caors::guid::from_bytes(caors::guid::byte_array{4})};
  if (assets.provider().bind(guid, source, "caors://assets/hello") ||
      assets.register_type(cache, loader, caors::string_name{"example.text"}))
    return 3;
  auto request = assets.create_group(worker.get_executor(), completion.get_executor())
                     .request_load<text_asset>(guid);
  // 每轮只消费当前任务，不能阻塞等待尚需本线程 pump 的宿主事件。
  for (int round = 0; round < 16 && !request.done(); ++round) {
    (void)worker.drain();
    (void)completion.drain();
  }
  if (!request.loaded() || assets.database().has_active_operations())
    return 4;
  std::cout << request.try_get()->text << '\n';
  return request.try_get()->text == "hello VFS" ? 0 : 5;
}

} // namespace

int main() {
  if (const auto status = run_example(false))
    return status;
  return run_example(true);
}
