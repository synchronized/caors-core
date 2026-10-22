// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/assets.h"
#include "caors/io.h"
#include "caors/resources/byte_reader.h"
#include "caors/resources/vfs_source.h"
#include "caors/tasks.h"

#include <iostream>

namespace {
class text_asset final : public caors::resource {
public:
  using caors::resource::resource;
  std::string text;
};

// 示例只解析 UTF-8 文本；真实格式校验、摘要复验和全局在途预算由宿主管理。
class text_loader final : public caors::resources::loader<text_asset> {
public:
  caors::error start(caors::resources::async_load_context context,
                     caors::resources::load_completion<text_asset> completion) override {
    auto sources = context.snapshot_as<caors::resources::provider_snapshot>();
    if (!sources)
      return {caors::error_domain::resources, caors::errc::unsupported};
    auto source = sources->resolve(context.guid);
    if (!source)
      return source.error();
    struct operation {
      caors::resources::async_load_context context;
      caors::resources::load_completion<text_asset> completion;
      caors::io::io_request<caors::io::byte_buffer> reading;
    };
    auto op = std::make_shared<operation>();
    op->context = std::move(context);
    op->completion = std::move(completion);
    auto read = caors::resources::request_bytes(
        reader_, {std::move(source.value), op->context.worker, 1024, op->context.resource},
        op->context.worker, [op](const auto& bytes) {
          caors::resources::load_result<text_asset> result;
          if (!bytes) {
            result.status = caors::resources::read_error(bytes.error());
          } else {
            auto value =
                caors::memory::make_unique<text_asset>(op->context.resource, op->context.guid);
            value->text.assign(reinterpret_cast<const char*>(bytes.value.data()),
                               bytes.value.size());
            result.value = std::move(value);
          }
          auto completion = std::move(op->completion);
          (void)completion.complete(std::move(result));
        });
    if (!read)
      return caors::resources::read_error(read.error());
    op->reading = std::move(read.value);
    return {};
  }

private:
  caors::resources::file_byte_reader reader_;
};

int run_example() {
  caors::tasks::execution_context worker;
  caors::tasks::execution_context completion;
  caors::resources::cache<text_asset> cache;
  text_loader loader;
  caors::assets::asset_manager assets{{worker.get_executor(), completion.get_executor()}};
  caors::io::vfs files;
  auto memory = caors::io::make_memory_backend();
  if (memory->store("hello", "hello VFS"))
    return 1;
  if (files.mount("caors://assets", memory))
    return 2;
  caors::resources::vfs_source source{files};
  const caors::resource_guid guid{caors::guid::from_bytes(caors::guid::byte_array{4})};
  if (caors::assets::project_builder{assets}.provider().bind(guid, source,
                                                             "caors://assets/hello") ||
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

int main() { return run_example(); }
