// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"

#include <cstddef>
#include <iostream>
#include <new>

namespace {

struct allocation_stats {
  int allocate_count = 0;
  int deallocate_count = 0;
};

void* callback_allocate(std::size_t size, std::size_t alignment, void* user_data) {
  auto* stats = static_cast<allocation_stats*>(user_data);
  ++stats->allocate_count;
  return ::operator new(size, std::align_val_t{alignment});
}

void callback_deallocate(void* ptr, std::size_t, std::size_t alignment, void* user_data) {
  auto* stats = static_cast<allocation_stats*>(user_data);
  ++stats->deallocate_count;
  ::operator delete(ptr, std::align_val_t{alignment});
}

} // namespace

int main() {
  allocation_stats stats;
  caors::callback_memory_resource resource(&callback_allocate, &callback_deallocate, &stats);

  constexpr std::size_t size = 256;
  constexpr std::size_t alignment = 32;

  void* ptr = caors::allocate(size, alignment, &resource);
  std::cout << "callback backend allocated " << size << " bytes at " << ptr << '\n';
  caors::deallocate(ptr, size, alignment, &resource);

  std::cout << "allocate count: " << stats.allocate_count << '\n';
  std::cout << "deallocate count: " << stats.deallocate_count << '\n';

  return 0;
}
