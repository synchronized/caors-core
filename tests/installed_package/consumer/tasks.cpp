// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks.h"

#include <iostream>

int main() {
  caors::tasks::execution_context context;
  int observed = 0;

  const caors::error status = context.get_executor().post(caors::tasks::task{[&observed] {
    observed = 7;
    return caors::error{};
  }});
  if (status || context.poll() != 1 || observed != 7) {
    std::cerr << "CAORS installed tasks consumer 失败\n";
    return 1;
  }

  std::cout << "CAORS installed tasks consumer passed\n";
  return 0;
}
