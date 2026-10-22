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
  return closing.result() && *closing.result() ? 0 : 5;
}
