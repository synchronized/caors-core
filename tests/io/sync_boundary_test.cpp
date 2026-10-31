// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/io/file_system.h"
#include "caors/io/memory_file_system.h"
#include "caors/io/native_file_system.h"

// 同步存储不能再通过包含链暴露异步执行和旧路由扩展。
#if defined(CAORS_IO_ASYNC_FILE_H_) || defined(CAORS_IO_ASYNC_REQUEST_H_) ||                       \
    defined(CAORS_IO_ROUTE_SNAPSHOT_H_)
#error "Synchronous storage headers must not expose asynchronous or route protocols"
#endif

#include "caors/io/async_file.h"
#include "caors/io/file_service.h"
#include "caors/tasks/execution_context.h"
#include "io_test_support.h"

namespace {
using namespace caors;
using namespace caors::io;
template <typename T>
concept has_route_snapshot = requires(const T& value) { value.snapshot(); };
template <typename T>
concept has_async_open = requires(T& value) {
  value.start_open(resolved_file{}, nullptr, io_completion<std::shared_ptr<async_file>>{});
};
static_assert(!has_route_snapshot<file_system>);
static_assert(!has_async_open<file_system>);
std::string contents(const io_result<byte_buffer>& bytes) {
  return {reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()};
}
}

TEST_CASE("io - memory service and synchronous view share storage", "[io][sync-boundary]") {
  auto storage = std::make_shared<memory_file_system>();
  auto adapted = make_memory_backend(storage);
  file_service service{{tasks::inline_executor()}};
  REQUIRE_FALSE(service.mount("data", adapted));
  REQUIRE_FALSE(storage->write_all("item", "sync"));
  auto read = service.read("data/item");
  REQUIRE(read.done());
  REQUIRE(*read.result());
  CHECK(contents(*read.result()) == "sync");

  auto bytes = make_byte_buffer();
  bytes.assign(3, std::byte{'a'});
  auto write = service.write("data/item", std::move(bytes));
  REQUIRE(write.done());
  REQUIRE(*write.result());
  auto synchronous = storage->read_all("item");
  REQUIRE(synchronous);
  CHECK(std::string(reinterpret_cast<const char*>(synchronous->data()), synchronous->size()) ==
        "aaa");
  CHECK_FALSE(make_memory_backend(std::shared_ptr<memory_file_system>{}));
}

TEST_CASE("io - worker backend shares native storage and explicitly schedules work",
          "[io][sync-boundary]") {
  const auto root = caors::tests::io::make_temp_dir();
  std::filesystem::create_directories(root);
  struct cleanup {
    std::filesystem::path root;
    ~cleanup() {
      std::error_code error;
      std::filesystem::remove_all(root, error);
    }
  } cleanup{root};
  auto storage = std::make_shared<native_file_system>(root.string());
  tasks::execution_context worker;
  auto adapted = make_worker_backend(storage, worker.get_executor());
  file_service service{{tasks::inline_executor()}};
  REQUIRE_FALSE(service.mount("data", adapted));
  REQUIRE_FALSE(storage->write_all("item", "native"));
  auto read = service.read("data/item");
  CHECK_FALSE(read.done());
  for (int i = 0; i < 32 && !read.done(); ++i)
    (void)worker.drain();
  REQUIRE(read.done());
  REQUIRE(*read.result());
  CHECK(contents(*read.result()) == "native");
  auto bytes = make_byte_buffer();
  bytes.assign(4, std::byte{'b'});
  auto write = service.write("data/item", std::move(bytes));
  CHECK_FALSE(write.done());
  (void)worker.drain();
  REQUIRE(write.done());
  REQUIRE(*write.result());
  auto synchronous = storage->read_all("item");
  REQUIRE(synchronous);
  CHECK(synchronous->size() == 4);
  CHECK((*synchronous)[0] == std::byte{'b'});
  CHECK_FALSE(make_worker_backend({}, worker.get_executor()));
  CHECK_FALSE(make_worker_backend(storage, {}));
}
