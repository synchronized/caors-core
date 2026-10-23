// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io.h"
#include "caors/tasks/execution_context.h"
#include <algorithm>
#include <iostream>

int main() {
  caors::io::vfs routes;
  auto memory = caors::io::make_memory_backend();
  const std::string text(100000, 'x');
  if (memory->store("payload", text) || routes.mount("assets", memory))
    return 1;
  caors::tasks::execution_context completion;
  caors::io::io_context io{routes, {completion.get_executor()}};
  // 此 CLI 示例使用即时内存后端，只排空本地通知；宿主实际接入应由自身事件循环推进。
  auto finish = [&](auto& request) {
    for (int frame = 0; frame < 32 && !request.done(); ++frame)
      (void)completion.drain();
    return request.result() && static_cast<bool>(*request.result());
  };
  auto opening = io.open("assets/payload", {.max_read_bytes = 4096});
  if (!finish(opening))
    return 2;
  auto file = opening.result()->value;
  std::uint64_t consumed = 0;
  while (consumed < file.size()) {
    auto length = (std::min)(std::uint64_t{4096}, file.size() - consumed);
    auto reading = file.read({.offset = consumed, .length = length});
    if (!finish(reading))
      return 3;
    for (auto byte : reading.result()->value)
      if (byte != std::byte{'x'})
        return 4;
    consumed += reading.result()->value.size();
  }
  auto closing = file.close();
  if (!finish(closing))
    return 5;
  auto all = io.read("assets/payload", {.max_bytes = text.size()});
  if (!finish(all) || all.result()->value.size() != text.size())
    return 6;
  std::cout << "consumed " << consumed << " bytes; aggregation " << all.result()->value.size()
            << " bytes\n";
}
