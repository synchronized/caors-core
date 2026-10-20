// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/config.h"
#include "io_test_support.h"

#include <atomic>
#include <thread>

namespace {
using namespace caors;
using namespace caors::io;
std::shared_ptr<memory_file_system> make_files(std::string_view text) {
  auto files = std::make_shared<memory_file_system>();
  REQUIRE(files->write_all("data", text).succeeded());
  return files;
}
std::string read_text(const resolved_file& file) {
  auto bytes = file.backend->read_all(file.path);
  REQUIRE(bytes.succeeded());
  return {reinterpret_cast<const char*>(bytes->data()), bytes->size()};
}
}

TEMPLATE_TEST_CASE("io - snapshot survives remount and router destruction", "[io][snapshot]",
                   mount_file_system, file_system_resolver) {
  route_snapshot old;
  std::uint64_t old_id = 0;
  std::weak_ptr<file_system> weak;
  {
    TestType router;
    auto original = make_files("old");
    weak = original;
    REQUIRE(router.mount("assets", std::move(original)).succeeded());
    auto captured = router.snapshot();
    REQUIRE(captured.succeeded());
    old = std::move(captured.value);
    auto first = old.resolve("assets/data");
    REQUIRE(first.succeeded());
    old_id = first->source_id;
    REQUIRE(old_id != 0);
    REQUIRE(router.unmount("assets"));
    REQUIRE(router.mount("assets", make_files("new")).succeeded());
    auto fresh = router.snapshot();
    REQUIRE(fresh.succeeded());
    auto second = fresh->resolve("assets/data");
    REQUIRE(second.succeeded());
    CHECK(second->source_id != old_id);
    CHECK(read_text(*second) == "new");
  }
  REQUIRE_FALSE(weak.expired());
  auto first = old.resolve("assets/data");
  REQUIRE(first.succeeded());
  CHECK(first->source_id == old_id);
  CHECK(read_text(*first) == "old");
  old = {};
  CHECK_FALSE(weak.expired());
  first = fail<resolved_file>({error_domain::io, errc::cancelled});
  CHECK(weak.expired());
}

TEST_CASE("io - nested snapshot freezes dependencies and preserves source identity",
          "[io][snapshot]") {
  auto mounts = std::make_shared<mount_file_system>();
  REQUIRE(mounts->mount("assets", make_files("old")).succeeded());
  file_system_resolver resolver;
  REQUIRE(resolver.mount("caors://app", mounts, "assets").succeeded());
  auto snapshot = resolver.snapshot();
  auto direct = mounts->snapshot();
  REQUIRE(snapshot.succeeded());
  REQUIRE(direct.succeeded());
  auto expected = direct->resolve("assets/data");
  auto actual = snapshot->resolve("caors://app/data");
  REQUIRE(expected.succeeded());
  REQUIRE(actual.succeeded());
  CHECK(expected->source_id == actual->source_id);
  mounts->clear();
  REQUIRE(mounts->mount("assets", make_files("new")).succeeded());
  CHECK(read_text(*snapshot->resolve("caors://app/data")) == "old");
  auto fresh = resolver.snapshot();
  REQUIRE(fresh.succeeded());
  CHECK(read_text(*fresh->resolve("caors://app/data")) == "new");
}

TEST_CASE("io - snapshots reject borrowed edges and diagnose nested cycles", "[io][snapshot]") {
  mount_file_system first;
  file_system_resolver second;
  memory_file_system borrowed;
  REQUIRE(first.mount("assets", borrowed).succeeded());
  CHECK(first.snapshot().error().value() == errc::unsupported);
  first.clear();
  REQUIRE(first.mount("assets", second).succeeded());
  REQUIRE(second.mount("nested", first).succeeded());
  CHECK(first.snapshot().error().value() == errc::unsupported);
  // 特意构造无效拥有式图验证诊断，测试自身随后拆环，不能依靠 shared_ptr 自动回收环。
  auto owned_first = std::make_shared<mount_file_system>();
  auto owned_second = std::make_shared<file_system_resolver>();
  REQUIRE_FALSE(owned_first->mount("assets", owned_second));
  REQUIRE_FALSE(owned_second->mount("nested", owned_first));
  CHECK(owned_first->snapshot().error().value() == errc::cycle_detected);
  owned_first->clear();
  owned_second->clear();
  CHECK_FALSE(route_snapshot{}.resolve("x"));
}

#if CAORS_HAS_BACKGROUND_THREADS
TEMPLATE_TEST_CASE("io - concurrent publication preserves independent mount updates",
                   "[io][snapshot]", mount_file_system, file_system_resolver) {
  TestType router;
  auto files = make_files("data");
  std::atomic<bool> valid{true};
  auto publish = [&](const char* prefix) {
    for (int index = 0; index < 100; ++index) {
      if (router.mount(prefix, files))
        valid = false;
      auto snapshot = router.snapshot();
      if (!snapshot) {
        valid = false;
        continue;
      }
      auto file = snapshot->resolve(std::string{prefix} + "/data");
      if (!file || file->path.string_view() != "data")
        valid = false;
      if (!router.unmount(prefix))
        valid = false;
    }
  };
  std::jthread first{publish, "first"};
  std::jthread second{publish, "second"};
  first.join();
  second.join();
  CHECK(valid.load());
  CHECK(router.mount_count() == 0);
}

#endif
