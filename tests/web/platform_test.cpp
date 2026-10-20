// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/config.h"
#include "caors/core/guid.h"
#include "caors/meta/detail/argument_access.h"
#include "caors/platform/shared_library.h"
#include "caors/tasks/thread_pool.h"
#include <catch2/catch_all.hpp>
#include <system_error>

TEST_CASE("Web 显式拒绝后台线程和动态加载", "[web]") {
  REQUIRE_FALSE(caors::config::has_background_threads);
  REQUIRE_FALSE(caors::tasks::thread_pool::supported());
  REQUIRE(caors::tasks::thread_pool::default_thread_count() == 0);
  REQUIRE_THROWS_AS(caors::tasks::thread_pool{}, std::system_error);
  auto library = caors::platform::shared_library::open("module.wasm");
  REQUIRE_FALSE(library);
  REQUIRE(library.error().value() == caors::errc::unsupported);
}
TEST_CASE("Web 保留浮点文本解析边界", "[web]") {
  long double value = 0;
  REQUIRE(caors::meta::detail::parse_floating_argument("1.25", value));
  REQUIRE(value == 1.25L);
  for (auto invalid : {"", "+1", " 1", "1 ", "nan", "inf", "1junk", "1e99999"}) {
    REQUIRE_FALSE(caors::meta::detail::parse_floating_argument(invalid, value));
  }
}
TEST_CASE("Web GUID 哈希显式折叠位宽", "[web]") {
  auto value = caors::guid::from_bytes(
      caors::guid::byte_array{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
  static_assert(sizeof(std::size_t) == 4);
  REQUIRE(caors::guid_hash{}(value) == 0xb221da7cU);
}
