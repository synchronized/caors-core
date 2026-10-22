// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/io/context.h"
#include "caors/tasks/execution_context.h"
#include <catch2/catch_all.hpp>
#if !defined(__EMSCRIPTEN__)
#include <chrono>
#include <filesystem>
#include <fstream>
#endif
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
class refusing_backend final : public backend {
public:
  io_error open(path, open_options, memory_resource*,
                io_completion<std::shared_ptr<async_file>>) override {
    return {io_errc::unsupported, 42, 7};
  }
};
}
TEST_CASE("io - VFS reads with one request and fixed routing per operation", "[io][vfs-context]") {
  tasks::execution_context completion;
  vfs routes;
  auto old = make_memory_backend();
  auto next = make_memory_backend();
  REQUIRE_FALSE(old->store("data", "old"));
  REQUIRE_FALSE(next->store("data", "new"));
  REQUIRE_FALSE(routes.mount("assets://game", old));
  CHECK(routes.mount("assets://game", next).value() == errc::already_exists);
  io_context io{routes, {completion.get_executor()}};
  int calls = 0;
  auto first = io.read("assets://game/data", {}, [&](const auto& value) {
    REQUIRE(value);
    CHECK(text(value) == "old");
    ++calls;
  });
  REQUIRE(routes.unmount("assets://game"));
  REQUIRE_FALSE(routes.mount("assets://game", next));
  auto second = io.read("assets://game/data");
  CHECK_FALSE(first.done());
  pump(first, completion);
  pump(second, completion);
  REQUIRE(first.result());
  REQUIRE(*first.result());
  REQUIRE(second.result());
  REQUIRE(*second.result());
  CHECK(text(*second.result()) == "new");
  CHECK(calls == 1);
}
TEST_CASE("io - explicit VFS batch outlives configuration and pins backend", "[io][vfs-context]") {
  tasks::execution_context completion;
  std::weak_ptr<backend> weak;
  vfs_snapshot snapshot;
  {
    vfs routes;
    auto memory = make_memory_backend();
    weak = memory;
    REQUIRE_FALSE(memory->store("data", "retained"));
    REQUIRE_FALSE(routes.mount("assets", memory));
    auto captured = routes.snapshot();
    REQUIRE(captured);
    snapshot = std::move(captured.value);
    routes.clear();
  }
  CHECK_FALSE(weak.expired());
  {
    io_context io{std::move(snapshot), {completion.get_executor()}};
    auto reading = io.read("assets/data");
    pump(reading, completion);
    REQUIRE(reading.result());
    REQUIRE(*reading.result());
    CHECK(text(*reading.result()) == "retained");
  }
  CHECK(weak.expired());
}
TEST_CASE("io - VFS open range read and close share a configured file", "[io][vfs-context]") {
  vfs routes;
  auto memory = make_memory_backend();
  REQUIRE_FALSE(memory->store("data", "abcdef"));
  REQUIRE_FALSE(routes.mount("files", memory));
  io_context io{routes, {tasks::inline_executor()}};
  auto opening = io.open("files/data", {.max_read_bytes = 3});
  REQUIRE(opening.done());
  REQUIRE(*opening.result());
  auto file = opening.result()->value;
  CHECK(file.size() == 6);
  auto reading = file.read({.offset = 2, .length = 3});
  REQUIRE(reading.done());
  REQUIRE(*reading.result());
  CHECK(text(*reading.result()) == "cde");
  auto excessive = file.read({.length = 4});
  REQUIRE(excessive.done());
  CHECK(excessive.result()->error().value == io_errc::limit_exceeded);
  auto copy = file;
  auto closed = file.close();
  REQUIRE(closed.done());
  REQUIRE(*closed.result());
  auto after = copy.read({.length = 1});
  REQUIRE(after.done());
  CHECK_FALSE(*after.result());
  auto again = copy.close();
  REQUIRE(again.done());
  REQUIRE(*again.result());
}
TEST_CASE("io - VFS start errors are delivered once through completion", "[io][vfs-context]") {
  tasks::execution_context completion;
  vfs routes;
  REQUIRE_FALSE(routes.mount("bad", std::make_shared<refusing_backend>()));
  io_context io{routes, {completion.get_executor()}};
  int calls = 0;
  auto refused = io.open("bad/data", {}, [&](const auto& value) {
    CHECK_FALSE(value);
    CHECK(value.error().backend_id == 42);
    ++calls;
  });
  CHECK_FALSE(refused.done());
  pump(refused, completion);
  REQUIRE(refused.result());
  CHECK(refused.result()->error().native_code == 7);
  CHECK(calls == 1);
  auto missing = io.read("missing/data");
  pump(missing, completion);
  REQUIRE(missing.result());
  CHECK(missing.result()->error().value == io_errc::not_found);
  auto invalid = io_context{routes, {}}.read("bad/data");
  REQUIRE(invalid.done());
  CHECK(invalid.result()->error().value == io_errc::completion_rejected);
}
TEST_CASE("io - VFS budget cancellation and queue discard remain observable", "[io][vfs-context]") {
  tasks::execution_context completion;
  vfs routes;
  auto memory = make_memory_backend();
  REQUIRE_FALSE(memory->store("data", "abc"));
  REQUIRE_FALSE(routes.mount("files", memory));
  io_context io{routes, {completion.get_executor()}};
  auto limited = io.read("files/data", {.max_bytes = 2});
  pump(limited, completion);
  REQUIRE(limited.result());
  CHECK(limited.result()->error().value == io_errc::limit_exceeded);
  auto cancelled = io.read("files/data");
  cancelled.cancel();
  pump(cancelled, completion);
  REQUIRE(cancelled.result());
  CHECK(cancelled.result()->error().value == io_errc::cancelled);
  auto dropped = io.open("files/data");
  completion.clear();
  completion.clear();
  pump(dropped, completion);
  REQUIRE(dropped.result());
  CHECK_FALSE(*dropped.result());
}
#if !defined(__EMSCRIPTEN__)
TEST_CASE("io - native backend binds worker at construction", "[io][vfs-context]") {
  const auto location =
      std::filesystem::temp_directory_path() /
      ("caors-vfs-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
       ".txt");
  {
    std::ofstream stream{location, std::ios::binary};
    stream << "native";
  }
  struct cleanup {
    std::filesystem::path location;
    ~cleanup() {
      std::error_code error;
      std::filesystem::remove(location, error);
    }
  } cleanup{location};
  tasks::execution_context worker, completion;
  vfs routes;
  auto directory = location.parent_path().generic_string();
  REQUIRE_FALSE(routes.mount("local", make_native_backend(directory, worker.get_executor())));
  io_context io{routes, {completion.get_executor()}};
  auto reading = io.read(std::string{"local/"} + location.filename().generic_string());
  CHECK_FALSE(reading.done());
  CHECK(worker.pending() == 1);
  for (int i = 0; i < 32 && !reading.done(); ++i) {
    (void)worker.drain();
    (void)completion.drain();
  }
  REQUIRE(reading.result());
  REQUIRE(*reading.result());
  CHECK(text(*reading.result()) == "native");
}
#endif

TEST_CASE("io - context open forwards cooperative cancellation to a host backend",
          "[io][vfs-context]") {
  class delayed_backend final : public backend {
  public:
    io_completion<std::shared_ptr<async_file>> pending;
    io_error open(path, open_options, memory_resource*,
                  io_completion<std::shared_ptr<async_file>> receipt) override {
      pending = std::move(receipt);
      return {};
    }
  };
  tasks::execution_context completion;
  vfs routes;
  auto host = std::make_shared<delayed_backend>();
  REQUIRE_FALSE(routes.mount("host", host));
  io_context io{routes, {completion.get_executor()}};
  auto opening = io.open("host/data");
  CHECK_FALSE(host->pending.stop_token().stop_requested());
  opening.cancel();
  CHECK(host->pending.stop_token().stop_requested());
  CHECK(host->pending.complete(io_failure<std::shared_ptr<async_file>>({io_errc::cancelled})));
  host->pending = {};
  pump(opening, completion);
  REQUIRE(opening.result());
  CHECK(opening.result()->error().value == io_errc::cancelled);
  std::weak_ptr<backend> weak = host;
  routes.clear();
  host.reset();
  CHECK(weak.expired());
}

TEST_CASE("io - opened files retain backend and module pin after unmount", "[io][vfs-context]") {
  file opened;
  std::weak_ptr<int> pin;
  {
    vfs routes;
    auto memory = make_memory_backend();
    auto module = std::make_shared<int>(1);
    pin = module;
    REQUIRE_FALSE(memory->store("data", "retained"));
    REQUIRE_FALSE(routes.mount("files", memory, {}, module));
    io_context io{routes, {tasks::inline_executor()}};
    auto opening = io.open("files/data");
    REQUIRE(opening.done());
    REQUIRE(*opening.result());
    opened = opening.result()->value;
    routes.clear();
  }
  CHECK_FALSE(pin.expired());
  {
    auto reading = opened.read({.length = 8});
    REQUIRE(reading.done());
    REQUIRE(*reading.result());
    CHECK(text(*reading.result()) == "retained");
    auto closing = opened.close();
    REQUIRE(closing.done());
    REQUIRE(*closing.result());
  }
  opened = {};
  CHECK(pin.expired());
}

TEST_CASE("io - context writes owned bytes and remaps metadata paths", "[io][vfs-context]") {
  tasks::execution_context completion;
  vfs routes;
  auto memory = make_memory_backend();
  REQUIRE_FALSE(routes.mount("assets://game", memory, "pack"));
  io_context io{routes, {completion.get_executor()}};
  auto bytes = make_byte_buffer();
  for (char c : std::string{"written"})
    bytes.push_back(static_cast<std::byte>(c));
  auto written = io.write("assets://game/data", std::move(bytes));
  pump(written, completion);
  REQUIRE(written.result());
  REQUIRE(*written.result());
  CHECK(written.result()->value == 7);
  auto status = io.stat("assets://game/data");
  pump(status, completion);
  REQUIRE(status.result());
  REQUIRE(*status.result());
  CHECK(status.result()->value.has_size);
  CHECK(status.result()->value.size == 7);
  auto entries = io.list("assets://game");
  pump(entries, completion);
  REQUIRE(entries.result());
  REQUIRE(*entries.result());
  REQUIRE(entries.result()->value.size() == 1);
  CHECK(entries.result()->value.front().name.string_view() == "data");
  CHECK(entries.result()->value.front().path.string_view() == "assets://game/data");
  REQUIRE_FALSE(memory->store("pack/with:colon", "colon"));
  auto names = io.list("assets://game");
  pump(names, completion);
  REQUIRE(names.result());
  REQUIRE(*names.result());
  bool found = false;
  for (const auto& entry : names.result()->value) {
    if (entry.name.string_view() == "with:colon") {
      found = true;
      CHECK(entry.path.string_view() == "assets://game/with:colon");
    }
  }
  CHECK(found);
  auto read = io.read("assets://game/data");
  pump(read, completion);
  REQUIRE(read.result());
  REQUIRE(*read.result());
  CHECK(text(*read.result()) == "written");
}
TEST_CASE("io - metadata optional capabilities report unsupported without fallback",
          "[io][vfs-context]") {
  vfs routes;
  REQUIRE_FALSE(routes.mount("readonly", std::make_shared<refusing_backend>()));
  io_context io{routes, {tasks::inline_executor()}};
  auto write = io.write("readonly/data", make_byte_buffer());
  REQUIRE(write.done());
  CHECK(write.result()->error().value == io_errc::unsupported);
  auto status = io.stat("readonly/data");
  REQUIRE(status.done());
  CHECK(status.result()->error().value == io_errc::unsupported);
  auto entries = io.list("readonly");
  REQUIRE(entries.done());
  CHECK(entries.result()->error().value == io_errc::unsupported);
}
#if !defined(__EMSCRIPTEN__)
TEST_CASE("io - native whole write cancellation and commit semantics", "[io][vfs-context]") {
  const auto location =
      std::filesystem::temp_directory_path() /
      ("caors-vfs-write-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt");
  struct cleanup {
    std::filesystem::path location;
    ~cleanup() {
      std::error_code error;
      std::filesystem::remove(location, error);
    }
  } cleanup{location};
  tasks::execution_context worker, completion;
  vfs routes;
  REQUIRE_FALSE(routes.mount("files", make_native_backend(location.parent_path().generic_string(),
                                                          worker.get_executor())));
  io_context io{routes, {completion.get_executor()}};
  auto logical = std::string{"files/"} + location.filename().generic_string();
  auto make = [] {
    auto bytes = make_byte_buffer();
    bytes.push_back(std::byte{42});
    return bytes;
  };
  auto cancelled = io.write(logical, make());
  cancelled.cancel();
  (void)worker.drain();
  pump(cancelled, completion);
  REQUIRE(cancelled.result());
  CHECK(cancelled.result()->error().value == io_errc::cancelled);
  CHECK_FALSE(std::filesystem::exists(location));
  auto written = io.write(logical, make());
  REQUIRE(worker.drain() == 1);
  CHECK(std::filesystem::exists(location));
  written.cancel();
  pump(written, completion);
  REQUIRE(written.result());
  CHECK(written.result()->error().value == io_errc::cancelled);
  CHECK(std::filesystem::file_size(location) == 1);
  auto status = io.stat(logical);
  (void)worker.drain();
  pump(status, completion);
  REQUIRE(status.result());
  REQUIRE(*status.result());
  CHECK(status.result()->value.size == 1);
}
#endif
#if defined(__linux__)
TEST_CASE("io - native buffered close failure is not reported as write success",
          "[io][vfs-context]") {
  vfs routes;
  REQUIRE_FALSE(routes.mount("local", make_native_backend("/", tasks::inline_executor())));
  io_context io{routes, {tasks::inline_executor()}};
  auto bytes = make_byte_buffer();
  bytes.push_back(std::byte{1});
  auto written = io.write("local/dev/full", std::move(bytes));
  REQUIRE(written.done());
  CHECK_FALSE(*written.result());
}
#endif

TEST_CASE("io - metadata operations retain backend until host cleanup and result release",
          "[io][vfs-context]") {
  class host_backend final : public backend {
  public:
    io_completion<file_status> pending;
    io_error open(path, open_options, memory_resource*,
                  io_completion<std::shared_ptr<async_file>>) override {
      return {io_errc::unsupported};
    }
    io_error stat(path, io_completion<file_status> receipt) override {
      pending = std::move(receipt);
      return {};
    }
  };
  tasks::execution_context completion;
  request<file_status> status;
  std::weak_ptr<host_backend> weak;
  {
    vfs routes;
    auto host = std::make_shared<host_backend>();
    weak = host;
    REQUIRE_FALSE(routes.mount("host", host));
    io_context io{routes, {completion.get_executor()}};
    status = io.stat("host/data");
    routes.clear();
    host.reset();
    auto retained = weak.lock();
    REQUIRE(retained);
    CHECK(retained->pending.complete({{}, {}}));
    retained->pending = {};
  }
  pump(status, completion);
  REQUIRE(status.result());
  REQUIRE(*status.result());
  CHECK_FALSE(weak.expired());
  status = {};
  CHECK(weak.expired());
}

TEST_CASE("io - logical directory enumeration rejects non-child backend names",
          "[io][vfs-context]") {
  class malformed_backend final : public backend {
  public:
    io_error open(path, open_options, memory_resource*,
                  io_completion<std::shared_ptr<async_file>>) override {
      return {io_errc::unsupported};
    }
    io_error list(path, memory_resource*, io_completion<directory_entry_list> receipt) override {
      directory_entry_list entries;
      entries.push_back({path{"../outside"}, path{"outside"}, file_kind::file});
      (void)receipt.complete({{}, std::move(entries)});
      return {};
    }
  };
  vfs routes;
  REQUIRE_FALSE(routes.mount("files", std::make_shared<malformed_backend>()));
  io_context io{routes, {tasks::inline_executor()}};
  auto listed = io.list("files");
  REQUIRE(listed.done());
  CHECK(listed.result()->error().value == io_errc::backend_failure);
}

TEST_CASE("io - whole write rejects a backend reporting incomplete success", "[io][vfs-context]") {
  class short_backend final : public backend {
  public:
    io_error open(path, open_options, memory_resource*,
                  io_completion<std::shared_ptr<async_file>>) override {
      return {io_errc::unsupported};
    }
    io_error write(path, byte_buffer, io_completion<std::uint64_t> receipt) override {
      (void)receipt.complete({{}, 0});
      return {};
    }
  };
  vfs routes;
  REQUIRE_FALSE(routes.mount("files", std::make_shared<short_backend>()));
  io_context io{routes, {tasks::inline_executor()}};
  auto bytes = make_byte_buffer();
  bytes.push_back(std::byte{42});
  auto written = io.write("files/data", std::move(bytes));
  REQUIRE(written.done());
  CHECK(written.result()->error().value == io_errc::backend_failure);
}
