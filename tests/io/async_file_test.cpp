// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks/execution_context.h"
#include "io_test_support.h"

#include <fstream>
#include <limits>

namespace {
using namespace caors;
using namespace caors::io;
std::shared_ptr<async_file> open_memory(std::string_view text) {
  auto fs = std::make_shared<memory_file_system>();
  REQUIRE(fs->write_all("data", text).succeeded());
  auto snapshot = route_snapshot::capture(fs);
  REQUIRE(snapshot);
  auto resolved = snapshot->resolve("data");
  REQUIRE(resolved);
  auto open = request_open(std::move(resolved.value), tasks::inline_executor());
  REQUIRE(open);
  auto result = open->result();
  REQUIRE(result);
  REQUIRE(*result);
  return result->value;
}
std::string text_of(const io_request<byte_buffer>& request) {
  auto result = request.result();
  REQUIRE(result);
  REQUIRE(*result);
  return {reinterpret_cast<const char*>(result->value.data()), result->value.size()};
}
class chunk_reader final : public random_access_reader {
public:
  std::uint64_t size() const noexcept override { return 8; }
  io_result<std::size_t> read_at(std::uint64_t offset, std::span<std::byte> buffer) override {
    ++reads;
    if (offset >= available)
      return {{}, 0};
    const auto count =
        (std::min)({buffer.size(), std::size_t{2}, available - static_cast<std::size_t>(offset)});
    for (std::size_t i = 0; i < count; ++i)
      buffer[i] = static_cast<std::byte>('a' + offset + i);
    return {{}, count};
  }
  io_error close() override {
    ++closes;
    return close_error;
  }
  int reads = 0;
  int closes = 0;
  std::size_t available = 8;
  io_error close_error{};
};
}
TEST_CASE("io - exact range validates bounds and budget before allocation", "[io][async-file]") {
  auto file = open_memory("abcdefgh");
  CHECK(file->source_id() != 0);
  auto executor = tasks::inline_executor();
  auto range = request_read(file, 2, 3, 3, executor);
  REQUIRE(range);
  CHECK(text_of(*range) == "cde");
  auto zero = request_read(file, 8, 0, 0, executor);
  REQUIRE(zero);
  REQUIRE(zero->result());
  CHECK(zero->result()->value.empty());
  CHECK(request_read(file, 9, 0, 1, executor).error().value == io_errc::out_of_range);
  CHECK(request_read(file, 1, (std::numeric_limits<std::uint64_t>::max)(),
                     (std::numeric_limits<std::uint64_t>::max)(), executor)
            .error()
            .value == io_errc::out_of_range);
  CHECK(request_read(file, 2, 4, 3, executor).error().value == io_errc::limit_exceeded);
  CHECK(read_all_bounded(file, 7, executor).error().value == io_errc::limit_exceeded);
}
TEST_CASE("io - worker accumulates short chunks and diagnoses premature EOF", "[io][async-file]") {
  auto reader = std::make_shared<chunk_reader>();
  auto file = make_worker_file(reader, tasks::inline_executor());
  auto full = request_read(file, 0, 8, 8, tasks::inline_executor());
  REQUIRE(full);
  CHECK(text_of(*full) == "abcdefgh");
  CHECK(reader->reads == 4);
  reader->available = 3;
  auto short_read = request_read(file, 0, 8, 8, tasks::inline_executor());
  REQUIRE(short_read);
  REQUIRE(short_read->result());
  CHECK(short_read->result()->error().value == io_errc::short_read);
  CHECK(short_read->result()->value.empty());
}
TEST_CASE("io - close waits for accepted reads and executes once", "[io][async-file]") {
  tasks::execution_context worker;
  auto reader = std::make_shared<chunk_reader>();
  reader->close_error = {io_errc::backend_failure, 57, 91};
  auto file = make_worker_file(reader, worker.get_executor());
  auto first = request_read(file, 0, 2, 2, tasks::inline_executor());
  auto second = request_read(file, 6, 2, 2, tasks::inline_executor());
  auto close = request_close(file, tasks::inline_executor());
  auto duplicate = request_close(file, tasks::inline_executor());
  REQUIRE(first);
  REQUIRE(second);
  REQUIRE(close);
  REQUIRE(duplicate);
  CHECK_FALSE(close->result());
  CHECK(request_read(file, 0, 1, 1, tasks::inline_executor()).error().value == io_errc::closed);
  REQUIRE(worker.drain() == 2);
  CHECK(text_of(*first) == "ab");
  CHECK(text_of(*second) == "gh");
  CHECK(reader->closes == 0);
  REQUIRE(worker.drain() == 1);
  REQUIRE(close->result());
  REQUIRE(duplicate->result());
  CHECK(close->result()->error().native_code == 91);
  CHECK(duplicate->result()->error().backend_id == 57);
  CHECK(reader->closes == 1);
  auto late = request_close(file, tasks::inline_executor());
  REQUIRE(late);
  REQUIRE(late->result());
  CHECK(late->result()->error().native_code == 91);
  CHECK(reader->closes == 1);
}
TEST_CASE("io - worker drop and cancellation retain cleanup without pending forever",
          "[io][async-file]") {
  tasks::execution_context worker;
  auto reader = std::make_shared<chunk_reader>();
  auto file = make_worker_file(reader, worker.get_executor());
  auto first = request_read(file, 0, 4, 4, tasks::inline_executor());
  auto second = request_read(file, 4, 4, 4, tasks::inline_executor());
  REQUIRE(first);
  REQUIRE(second);
  first->cancel();
  REQUIRE(worker.drain() == 2);
  REQUIRE(first->result());
  CHECK(first->result()->error().value == io_errc::cancelled);
  CHECK(text_of(*second) == "efgh");
  auto dropped = request_read(file, 0, 4, 4, tasks::inline_executor());
  REQUIRE(dropped);
  worker.clear();
  REQUIRE(dropped->result());
  CHECK(dropped->result()->error().value == io_errc::cancelled);
  auto close = request_close(file, tasks::inline_executor());
  REQUIRE(close);
  worker.clear();
  REQUIRE(close->result());
  CHECK(close->result()->error().value == io_errc::closed);
}
TEST_CASE("io - native file small ranges use explicit worker adapter", "[io][async-file]") {
  const auto root = caors::tests::io::make_temp_dir();
  std::filesystem::create_directories(root);
  struct remove_directory {
    std::filesystem::path path;
    ~remove_directory() {
      std::error_code ec;
      std::filesystem::remove_all(path, ec);
    }
  } cleanup{root};
  {
    std::ofstream output{root / "data", std::ios::binary};
    output << "abcdefgh";
  }
  auto fs = std::make_shared<native_file_system>(caors::tests::io::path_string(root));
  auto snapshot = route_snapshot::capture(fs);
  REQUIRE(snapshot);
  auto resolved = snapshot->resolve("data");
  REQUIRE(resolved);
  CHECK(request_open(resolved.value, tasks::inline_executor()).error().value ==
        io_errc::unsupported);
  auto opened = request_open_on_worker(std::move(resolved.value), tasks::inline_executor(),
                                       tasks::inline_executor());
  REQUIRE(opened);
  REQUIRE(opened->result());
  REQUIRE(*opened->result());
  auto file = opened->result()->value;
  auto first = request_read(file, 5, 3, 3, tasks::inline_executor());
  auto second = request_read(file, 0, 2, 2, tasks::inline_executor());
  REQUIRE(first);
  REQUIRE(second);
  CHECK(text_of(*first) == "fgh");
  CHECK(text_of(*second) == "ab");
  auto close = request_close(file, tasks::inline_executor());
  REQUIRE(close);
  REQUIRE(close->result());
  CHECK(*close->result());
}

namespace {
class immediate_file final : public async_file {
public:
  std::uint64_t size() const noexcept override { return 1; }
  io_error start_read(std::uint64_t, std::uint64_t length, memory_resource* resource,
                      io_completion<byte_buffer> completion) override {
    auto bytes = make_byte_buffer(resource);
    bytes.resize(static_cast<std::size_t>(length), std::byte{7});
    (void)completion.complete({{}, std::move(bytes)});
    return {};
  }
  io_error start_close(io_completion<close_result> completion) override {
    (void)completion.complete({{}, {}});
    return {};
  }
};
class delayed_open_backend final : public file_system {
public:
  io_error start_open(resolved_file, memory_resource*,
                      io_completion<std::shared_ptr<async_file>> completion) override {
    pending = std::move(completion);
    return {};
  }
  bool exists(path_view) const override { return true; }
  byte_buffer_result read_all(path_view, memory_resource*) override {
    return fail<byte_buffer>({error_domain::io, errc::unsupported});
  }
  error write_all(path_view, std::span<const std::byte>) override {
    return {error_domain::io, errc::unsupported};
  }
  void finish() {
    auto receipt = std::move(pending);
    (void)receipt.complete({{}, std::make_shared<immediate_file>()});
  }
  io_completion<std::shared_ptr<async_file>> pending;
};
}
TEST_CASE("io - native opening and delivered file pin backend beyond unmount", "[io][async-file]") {
  io_request<std::shared_ptr<async_file>> request;
  std::weak_ptr<delayed_open_backend> weak;
  std::weak_ptr<int> pin;
  {
    mount_file_system router;
    auto backend = std::make_shared<delayed_open_backend>();
    auto lifetime = std::make_shared<int>(9);
    weak = backend;
    pin = lifetime;
    REQUIRE_FALSE(router.mount("assets", std::move(backend), {}, std::move(lifetime)));
    auto snapshot = router.snapshot();
    REQUIRE(snapshot);
    auto file = snapshot->resolve("assets/data");
    REQUIRE(file);
    auto opened = request_open(std::move(file.value), tasks::inline_executor());
    REQUIRE(opened);
    request = std::move(opened.value);
    router.clear();
  }
  REQUIRE_FALSE(weak.expired());
  REQUIRE_FALSE(pin.expired());
  weak.lock()->finish();
  auto result = request.result();
  REQUIRE(result);
  REQUIRE(*result);
  auto file = result->value;
  result.reset();
  request = {};
  CHECK_FALSE(weak.expired());
  CHECK_FALSE(pin.expired());
  auto read = request_read(file, 0, 1, 1, tasks::inline_executor());
  REQUIRE(read);
  REQUIRE(read->result());
  CHECK(*read->result());
  read = io_failure<io_request<byte_buffer>>({io_errc::cancelled});
  file.reset();
  CHECK(weak.expired());
  CHECK(pin.expired());
}

TEST_CASE("io - worker file destructor schedules nonblocking fallback close", "[io][async-file]") {
  tasks::execution_context worker;
  auto reader = std::make_shared<chunk_reader>();
  auto file = make_worker_file(reader, worker.get_executor());
  file.reset();
  CHECK(reader->closes == 0);
  REQUIRE(worker.drain() == 1);
  CHECK(reader->closes == 1);
}

namespace {
class malformed_file final : public async_file {
public:
  std::uint64_t size() const noexcept override { return 8; }
  io_error start_read(std::uint64_t, std::uint64_t, memory_resource* resource,
                      io_completion<byte_buffer> receipt) override {
    auto bytes = make_byte_buffer(resource);
    bytes.resize(result_length);
    (void)receipt.complete({{}, std::move(bytes)});
    return {};
  }
  io_error start_close(io_completion<close_result> receipt) override {
    (void)receipt.complete({{}, {}});
    return {};
  }
  std::size_t result_length = 2;
};
}
TEST_CASE("io - native success must deliver exactly the requested byte count", "[io][async-file]") {
  auto file = std::make_shared<malformed_file>();
  auto short_read = request_read(file, 0, 3, 3, tasks::inline_executor());
  REQUIRE(short_read);
  REQUIRE(short_read->result());
  CHECK(short_read->result()->error().value == io_errc::short_read);
  file->result_length = 4;
  auto oversized = request_read(file, 0, 3, 3, tasks::inline_executor());
  REQUIRE(oversized);
  REQUIRE(oversized->result());
  CHECK(oversized->result()->error().value == io_errc::backend_failure);
}
