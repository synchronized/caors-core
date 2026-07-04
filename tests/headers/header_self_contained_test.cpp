// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include <catch2/catch_all.hpp>

TEST_CASE("headers - public headers are self-contained", "[headers]") {
  SUCCEED("公开头文件自包含检查在编译阶段完成");
}
