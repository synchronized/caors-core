// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include <caors/tasks.h>
#include <caors/version.h>
int main() {
  return caors::get_version() == caors::k_version && !caors::tasks::thread_pool::supported() ? 0
                                                                                             : 1;
}
