// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/io/context.h"
#include "caors/tasks/execution_context.h"
#include <iostream>
int main() {
  caors::tasks::execution_context completion;
  caors::io::vfs files;
  auto memory = caors::io::make_memory_backend();
  if (memory->store("hello.txt", "hello VFS context") || files.mount("assets://game", memory))
    return 1;
  caors::io::io_context io{files, {.completion = completion.get_executor()}};
  auto reading = io.read("assets://game/hello.txt", {.max_bytes = 1024});
  for (int frame = 0; frame < 32 && !reading.done(); ++frame)
    (void)completion.drain();
  auto result = reading.result();
  if (!result || !*result)
    return 2;
  std::cout.write(reinterpret_cast<const char*>(result->value.data()), result->value.size());
  std::cout << '\n';
  auto opening = io.open("assets://game/hello.txt", {.access = caors::io::file_access::read});
  for (int frame = 0; frame < 32 && !opening.done(); ++frame)
    (void)completion.drain();
  if (!opening.result() || !*opening.result())
    return 3;
  auto file = opening.result()->value;
  auto range = file.read({.offset = 6, .length = 3});
  for (int frame = 0; frame < 32 && !range.done(); ++frame)
    (void)completion.drain();
  if (!range.result() || !*range.result())
    return 4;
  auto closing = file.close();
  for (int frame = 0; frame < 32 && !closing.done(); ++frame)
    (void)completion.drain();
  if (!closing.result() || !*closing.result())
    return 5;
  auto bytes = caors::io::make_byte_buffer();
  bytes.push_back(std::byte{42});
  auto written = io.write("assets://game/created.bin", std::move(bytes));
  for (int frame = 0; frame < 32 && !written.done(); ++frame)
    (void)completion.drain();
  if (!written.result() || !*written.result())
    return 6;
  auto status = io.stat("assets://game/created.bin");
  for (int frame = 0; frame < 32 && !status.done(); ++frame)
    (void)completion.drain();
  if (!status.result() || !*status.result() || status.result()->value.size != 1)
    return 7;
  auto entries = io.list("assets://game");
  for (int frame = 0; frame < 32 && !entries.done(); ++frame)
    (void)completion.drain();
  return entries.result() && *entries.result() && entries.result()->value.size() == 2 ? 0 : 8;
}
