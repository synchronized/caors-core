// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io.h"
#include "caors/tasks/execution_context.h"
#include <iostream>

int main() {
  auto files = std::make_shared<caors::io::memory_file_system>();
  const std::string text(100000, 'x');
  if (files->write_all("payload", text))
    return 1;
  auto snapshot = caors::io::route_snapshot::capture(files);
  if (!snapshot)
    return 2;
  auto resolved = snapshot->resolve("payload");
  if (!resolved)
    return 3;
  auto open = caors::io::request_open(std::move(resolved.value), caors::tasks::inline_executor());
  if (!open || !open->result() || !*open->result())
    return 4;
  auto file = open->result()->value;
  caors::tasks::execution_context completion;
  std::uint64_t consumed = 0;
  auto stream = caors::io::consume_chunks(
      file, {text.size(), 4096}, completion.get_executor(), [&](auto offset, auto block) {
        if (offset != consumed)
          return caors::io::io_error{caors::io::io_errc::backend_failure};
        for (auto byte : block)
          if (byte != std::byte{'x'})
            return caors::io::io_error{caors::io::io_errc::backend_failure};
        consumed += block.size();
        return caors::io::io_error{};
      });
  if (!stream)
    return 5;
  while (!stream->result())
    if (completion.drain() == 0)
      return 6;
  if (!*stream->result() || consumed != text.size())
    return 7;
  // 聚合与消费共用文件；helper 不自动关闭，文件由调用者统一收口。
  auto all =
      caors::io::read_all_chunked(file, {text.size(), 8192}, caors::tasks::inline_executor());
  if (!all || !all->result() || !*all->result() || all->result()->value.size() != text.size())
    return 8;
  auto close = caors::io::request_close(file, caors::tasks::inline_executor());
  if (!close || !close->result() || !*close->result())
    return 9;
  std::cout << "consumed " << consumed << " bytes; aggregation " << all->result()->value.size()
            << " bytes\n";
}
