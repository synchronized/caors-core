// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/io/async_file.h"
#include "caors/io/context.h"
#include "caors/tasks/execution_context.h"
#include <algorithm>
#include <cstdio>
#if !defined(__EMSCRIPTEN__)
#include <chrono>
#include <filesystem>
#include <fstream>
#endif
#include <catch2/catch_all.hpp>
namespace {
using namespace caors;
using namespace caors::io;
std::string text(const io_result<byte_buffer>& value) {
  return {reinterpret_cast<const char*>(value.value.data()), value.value.size()};
}
template <typename T> void pump(request<T>& value, tasks::execution_context& completion) {
  for (int i = 0; i < 32 && !value.done(); ++i)
    (void)completion.drain();
}
class short_writer final : public random_access_reader {
public:
  std::size_t calls = 0;
  std::size_t closes = 0;
  std::uint64_t size() const noexcept override { return 0; }
  io_result<std::size_t> read_at(std::uint64_t, std::span<std::byte>) override {
    return io_failure<std::size_t>({io_errc::unsupported});
  }
  io_result<std::size_t> write_at(std::uint64_t, std::span<const std::byte> bytes) override {
    ++calls;
    return {{}, (std::min)(bytes.size(), std::size_t{2})};
  }
  io_error close() override {
    ++closes;
    return {};
  }
};
class writer_backend final : public backend {
public:
  std::shared_ptr<short_writer> storage = std::make_shared<short_writer>();
  tasks::executor worker;
  io_error open(path, open_options, memory_resource*,
                io_completion<std::shared_ptr<async_file>> receipt) override {
    (void)receipt.complete({{}, make_worker_file(storage, worker)});
    return {};
  }
};
}
TEST_CASE("io - writable worker drains accepted work before shared close", "[io][vfs-write]") {
  tasks::execution_context worker, completion;
  auto target = std::make_shared<writer_backend>();
  target->worker = worker.get_executor();
  vfs routes;
  REQUIRE_FALSE(routes.mount("data", target));
  io_context io{routes, {completion.get_executor()}};
  auto opening = io.open("data/file", {.access = file_access::write});
  for (int i = 0; i < 8 && !opening.done(); ++i)
    (void)completion.drain();
  REQUIRE(opening.result());
  REQUIRE(*opening.result());
  auto handle = opening.result()->value;
  auto bytes = make_byte_buffer();
  bytes.resize(4);
  auto writing = handle.write({}, std::move(bytes));
  auto closing = handle.close();
  CHECK_FALSE(closing.done());
  auto late_bytes = make_byte_buffer();
  late_bytes.resize(1);
  auto late = handle.write({}, std::move(late_bytes));
  bool cancel = false;
  bool discard = false;
  SECTION("short writes are accumulated") {}
  SECTION("cancelled queued writes do not touch storage") {
    cancel = true;
    writing.cancel();
  }
  SECTION("discarded queued writes still allow close cleanup") {
    discard = true;
    worker.clear();
  }
  for (int i = 0; i < 16 && (!writing.done() || !closing.done() || !late.done()); ++i) {
    (void)worker.drain();
    (void)completion.drain();
  }
  REQUIRE(writing.done());
  REQUIRE(closing.done());
  REQUIRE(late.done());
  REQUIRE(*closing.result());
  CHECK(late.result()->error().value == io_errc::closed);
  CHECK(target->storage->closes == 1);
  if (cancel || discard) {
    CHECK_FALSE(*writing.result());
    CHECK(target->storage->calls == 0);
  } else {
    REQUIRE(*writing.result());
    CHECK(writing.result()->value == 4);
    CHECK(target->storage->calls == 2);
    CHECK(handle.size() == 4);
  }
}

TEST_CASE("io - writable memory open shares close and publishes bounded ranges",
          "[io][vfs-context]") {
  vfs routes;
  auto backend = make_memory_backend();
  REQUIRE_FALSE(routes.mount("data", backend));
  tasks::execution_context completion;
  io_context io{routes, {completion.get_executor()}};
  auto rejected = io.open("data/missing", {.access = file_access::write});
  pump(rejected, completion);
  REQUIRE(rejected.result());
  CHECK(rejected.result()->error().value == io_errc::not_found);
  auto invalid = io.open("data/file", {.creation = file_creation::create_always});
  pump(invalid, completion);
  CHECK(invalid.result()->error().value == io_errc::invalid_argument);
  auto opened = io.open("data/file", {.access = file_access::read_write,
                                      .creation = file_creation::create_always,
                                      .max_write_bytes = 3});
  pump(opened, completion);
  REQUIRE(*opened.result());
  auto handle = opened.result()->value;
  auto copy = handle;
  auto make = [](std::string_view text) {
    auto bytes = make_byte_buffer();
    for (char c : text)
      bytes.push_back(static_cast<std::byte>(c));
    return bytes;
  };
  auto limit = handle.write({}, make("long"));
  pump(limit, completion);
  CHECK(limit.result()->error().value == io_errc::limit_exceeded);
  auto first = handle.write({}, make("abc"));
  pump(first, completion);
  REQUIRE(*first.result());
  CHECK(handle.size() == 3);
  auto empty = handle.write({100}, make_byte_buffer());
  pump(empty, completion);
  REQUIRE(*empty.result());
  CHECK(handle.size() == 3);
  auto overflow = handle.write({(std::numeric_limits<std::uint64_t>::max)()}, make("x"));
  pump(overflow, completion);
  CHECK(overflow.result()->error().value == io_errc::out_of_range);

  auto second = handle.write({1}, make("Z"));
  pump(second, completion);
  REQUIRE(*second.result());
  auto read = handle.read({0, 3});
  pump(read, completion);
  REQUIRE(*read.result());
  CHECK(text(*read.result()) == "aZc");
  auto closed = handle.close();
  pump(closed, completion);
  REQUIRE(*closed.result());
  auto after = copy.write({}, make("x"));
  pump(after, completion);
  CHECK(after.result()->error().value == io_errc::closed);
  auto repeat = copy.close();
  pump(repeat, completion);
  REQUIRE(*repeat.result());
  auto published = io.read("data/file");
  pump(published, completion);
  REQUIRE(*published.result());
  CHECK(text(*published.result()) == "aZc");
  auto readonly = io.open("data/file");
  pump(readonly, completion);
  REQUIRE(*readonly.result());
  auto denied_write = readonly.result()->value.write({}, make_byte_buffer());
  pump(denied_write, completion);
  CHECK(denied_write.result()->error().value == io_errc::permission_denied);
  auto only_write = io.open("data/file", {.access = file_access::write});
  pump(only_write, completion);
  REQUIRE(*only_write.result());
  auto denied_read = only_write.result()->value.read({0, 0});
  pump(denied_read, completion);
  CHECK(denied_read.result()->error().value == io_errc::permission_denied);
  auto close_read = readonly.result()->value.close();
  pump(close_read, completion);
  REQUIRE_FALSE(backend->store("file", "fresh"));
  auto close_write = only_write.result()->value.close();
  pump(close_write, completion);
  auto fresh = io.read("data/file");
  pump(fresh, completion);
  REQUIRE(*fresh.result());
  CHECK(text(*fresh.result()) == "fresh");
}
#if !defined(__EMSCRIPTEN__)
TEST_CASE("io - native writable open does not truncate existing unless explicitly requested",
          "[io][vfs-context]") {
  auto root = std::filesystem::temp_directory_path() /
              ("caors-vfs-write-" +
               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  struct cleanup {
    std::filesystem::path path;
    ~cleanup() {
      std::filesystem::remove(path / "file");
      std::filesystem::remove(path);
    }
  } guard{root};
  {
    std::ofstream output{root / "file", std::ios::binary};
    output << "abc";
  }
  tasks::execution_context worker, completion;
  vfs routes;
  REQUIRE_FALSE(routes.mount("data", make_native_backend(root.string(), worker.get_executor())));
  io_context io{routes, {completion.get_executor()}};
  auto drain = [&](auto& value) {
    for (int i = 0; i < 32 && !value.done(); ++i) {
      (void)worker.drain();
      (void)completion.drain();
    }
  };
  auto opened = io.open("data/file", {.access = file_access::read_write});
  drain(opened);
  REQUIRE(*opened.result());
  auto handle = opened.result()->value;
  auto bytes = make_byte_buffer();
  bytes.push_back(std::byte{'X'});
  auto written = handle.write({1}, std::move(bytes));
  drain(written);
  REQUIRE(*written.result());
  auto read = handle.read({0, 3});
  drain(read);
  REQUIRE(*read.result());
  CHECK(text(*read.result()) == "aXc");
  auto closed = handle.close();
  drain(closed);
  REQUIRE(*closed.result());
  auto missing = io.open("data/missing", {.access = file_access::write});
  drain(missing);
  CHECK(missing.result()->error().value == io_errc::not_found);
  CHECK_FALSE(std::filesystem::exists(root / "missing"));
  auto create = io.open("data/file",
                        {.access = file_access::write, .creation = file_creation::create_always});
  drain(create);
  REQUIRE(*create.result());
  CHECK(create.result()->value.size() == 0);
  auto close = create.result()->value.close();
  drain(close);
  REQUIRE(*close.result());
  CHECK(std::filesystem::file_size(root / "file") == 0);
}
#endif

#if defined(__EMSCRIPTEN__)
TEST_CASE("io - Emscripten native adapter writes virtual filesystem through an explicit worker",
          "[io][vfs-write][web]") {
  struct cleanup {
    ~cleanup() { (void)std::remove("caors-web-write.bin"); }
  } guard;
  tasks::execution_context worker, completion;
  vfs routes;
  REQUIRE_FALSE(routes.mount("data", make_native_backend({}, worker.get_executor())));
  io_context io{routes, {completion.get_executor()}};
  auto drain = [&](auto& value) {
    for (int i = 0; i < 16 && !value.done(); ++i) {
      (void)worker.drain();
      (void)completion.drain();
    }
  };
  auto opened = io.open("data/caors-web-write.bin", {.access = file_access::read_write,
                                                     .creation = file_creation::create_always});
  drain(opened);
  REQUIRE(opened.result());
  REQUIRE(*opened.result());
  auto bytes = make_byte_buffer();
  bytes.push_back(std::byte{43});
  auto written = opened.result()->value.write({}, std::move(bytes));
  drain(written);
  REQUIRE(written.result());
  REQUIRE(*written.result());
  auto closed = opened.result()->value.close();
  drain(closed);
  REQUIRE(closed.result());
  REQUIRE(*closed.result());
  auto read = io.read("data/caors-web-write.bin");
  drain(read);
  REQUIRE(read.result());
  REQUIRE(*read.result());
  REQUIRE(read.result()->value.size() == 1);
  CHECK(read.result()->value[0] == std::byte{43});
}
#endif
