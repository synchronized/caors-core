// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"

#include <cstddef>
#include <iostream>

int main() {
  constexpr std::size_t size = 128;
  constexpr std::size_t alignment = 64;

  void* ptr = caors::allocate(size, alignment);
  std::cout << "system backend allocated " << size << " bytes at " << ptr << '\n';
  caors::deallocate(ptr, size, alignment);

  return 0;
}
