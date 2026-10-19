// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "io_test_support.h"

#include <memory>
#include <string>
#include <vector>

namespace {
struct observation {
  std::vector<int> released;
  bool backend_outlived_pin = false;
};
struct lifetime_probe {
  explicit lifetime_probe(observation& value) : seen(value) {}
  ~lifetime_probe() { seen.released.push_back(2); }
  observation& seen;
};
class observed_backend final : public caors::io::file_system {
public:
  observed_backend(observation& seen, std::weak_ptr<void> lifetime)
      : seen_(seen), lifetime_(std::move(lifetime)) {}
  ~observed_backend() override {
    seen_.backend_outlived_pin = lifetime_.expired();
    seen_.released.push_back(1);
  }
  bool exists(caors::io::path_view path) const override { return files.exists(path); }
  caors::io::byte_buffer_result read_all(caors::io::path_view path,
                                         caors::memory_resource* resource) override {
    return files.read_all(path, resource);
  }
  caors::error write_all(caors::io::path_view path, std::span<const std::byte> bytes) override {
    return files.write_all(path, bytes);
  }
  caors::io::memory_file_system files;

private:
  observation& seen_;
  std::weak_ptr<void> lifetime_;
};
} // namespace

TEMPLATE_TEST_CASE("io - owned mount retains backend and releases it before module lifetime",
                   "[io][owned-mount]", caors::io::mount_file_system,
                   caors::io::file_system_resolver) {
  observation seen;
  auto pin = std::make_shared<lifetime_probe>(seen);
  auto backend = std::make_shared<observed_backend>(seen, pin);
  REQUIRE(backend->files.write_all("root/data", std::string_view{"payload"}).succeeded());
  std::weak_ptr<observed_backend> weak = backend;
  TestType router;
  REQUIRE(router.mount("assets", std::move(backend), "root", std::move(pin)).succeeded());
  REQUIRE_FALSE(weak.expired());
  REQUIRE(router.exists("assets/data"));
  auto bytes = router.read_all("assets/data");
  REQUIRE(bytes.succeeded());
  CHECK(bytes->size() == 7);
  // 触发容器扩容和 erase 的元素移动，模块凭据不能在旧后端析构前释放。
  auto other = std::make_shared<caors::io::memory_file_system>();
  for (int index = 0; index < 20; ++index) {
    REQUIRE(router.mount(std::string{"extra"} + std::to_string(index), other).succeeded());
  }
  CHECK(seen.released.empty());
  REQUIRE(router.unmount("assets"));
  CHECK(weak.expired());
  CHECK_FALSE(seen.backend_outlived_pin);
  CHECK(seen.released == std::vector<int>{1, 2});
  CHECK(router.mount_count() == 20);
}

TEMPLATE_TEST_CASE("io - clear and router destruction release owned backend safely",
                   "[io][owned-mount]", caors::io::mount_file_system,
                   caors::io::file_system_resolver) {
  const bool explicit_clear = GENERATE(false, true);
  observation seen;
  std::weak_ptr<observed_backend> weak;
  {
    TestType router;
    auto pin = std::make_shared<lifetime_probe>(seen);
    auto backend = std::make_shared<observed_backend>(seen, pin);
    weak = backend;
    REQUIRE(router.mount("assets", std::move(backend), {}, std::move(pin)).succeeded());
    if (explicit_clear) {
      router.clear();
      CHECK(router.mount_count() == 0);
      CHECK(weak.expired());
    }
  }
  CHECK(weak.expired());
  CHECK_FALSE(seen.backend_outlived_pin);
  CHECK(seen.released == std::vector<int>{1, 2});
}

TEMPLATE_TEST_CASE("io - rejected owned mount releases candidate and preserves existing route",
                   "[io][owned-mount]", caors::io::mount_file_system,
                   caors::io::file_system_resolver) {
  TestType router;
  caors::io::memory_file_system original;
  REQUIRE(original.write_all("data", std::string_view{"old"}).succeeded());
  REQUIRE(router.mount("assets", original).succeeded());
  observation seen;
  auto pin = std::make_shared<lifetime_probe>(seen);
  auto candidate = std::make_shared<observed_backend>(seen, pin);
  std::weak_ptr<observed_backend> weak = candidate;
  CHECK(router.mount("assets", std::move(candidate), {}, std::move(pin)).value() ==
        caors::errc::already_exists);
  CHECK(weak.expired());
  CHECK_FALSE(seen.backend_outlived_pin);
  CHECK(seen.released == std::vector<int>{1, 2});
  CHECK(router.mount_count() == 1);
  CHECK(router.exists("assets/data"));
  CHECK(router.mount("empty", std::shared_ptr<caors::io::file_system>{}).value() ==
        caors::errc::invalid_argument);
  CHECK(router.mount_count() == 1);
}

#if CAORS_HAS_EXCEPTIONS
namespace {
class failing_mount_resource final : public caors::memory_resource {
public:
  bool armed = false;
  std::size_t remaining = 0;

private:
  void* do_try_allocate(std::size_t size, std::size_t alignment) noexcept override {
    // MSVC Debug 的小型容器代理在 noexcept 构造中分配，本专项只注入控制块及负载分配失败。
    if (armed && size > 32 && remaining-- == 0) {
      return nullptr;
    }
    return upstream_.try_allocate(size, alignment);
  }
  void do_deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept override {
    upstream_.deallocate(ptr, size, alignment);
  }
  bool do_is_equal(const caors::memory_resource& other) const noexcept override {
    return this == &other;
  }
  caors::memory::system_memory_resource upstream_;
};
} // namespace

TEMPLATE_TEST_CASE("io - owned mount allocation failure preserves routes and lifetime order",
                   "[io][owned-mount]", caors::io::mount_file_system,
                   caors::io::file_system_resolver) {
  const auto fail_at = GENERATE(0u, 1u, 2u, 3u, 4u, 5u, 6u);
  failing_mount_resource resource;
  observation seen;
  TestType router{&resource};
  caors::io::memory_file_system original;
  REQUIRE(original.write_all("data", std::string_view{"old"}).succeeded());
  REQUIRE(router.mount("existing", original).succeeded());
  auto pin = std::make_shared<lifetime_probe>(seen);
  auto backend = std::make_shared<observed_backend>(seen, pin);
  std::weak_ptr<observed_backend> weak = backend;
  resource.armed = true;
  resource.remaining = fail_at;
  bool failed = false;
  caors::error status;
  try {
    status =
        router.mount("new-prefix-long-enough-to-force-route-string-allocation", std::move(backend),
                     "target-prefix-long-enough-to-force-allocation", std::move(pin));
  } catch (const std::bad_alloc&) {
    failed = true;
  }
  if (!failed) {
    REQUIRE(status.succeeded());
  }
  resource.armed = false;
  CHECK(router.mount_count() == (failed ? 1 : 2));
  CHECK(router.exists("existing/data"));
  if (failed) {
    CHECK(weak.expired());
  }
  router.clear();
  CHECK(weak.expired());
  CHECK_FALSE(seen.backend_outlived_pin);
  CHECK(seen.released == std::vector<int>{1, 2});
}
#endif
