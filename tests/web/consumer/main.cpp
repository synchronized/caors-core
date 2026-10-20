// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include <caors/assets.h>
#include <caors/tasks.h>
#include <caors/version.h>
int main() {
  caors::tasks::execution_context context;
  caors::assets::asset_manager manager{{context.get_executor(), context.get_executor()}};
  return caors::get_version() == caors::k_version && !caors::tasks::thread_pool::supported() ? 0
                                                                                             : 1;
}
