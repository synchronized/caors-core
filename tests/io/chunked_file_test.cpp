// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/chunked_file.h"
#include "caors/tasks/execution_context.h"
#include "caors/tasks/thread_pool.h"
#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <thread>

using namespace caors;
using namespace caors::io;
namespace {
class pattern_file final : public async_file {
public:
  std::uint64_t bytes = 100;
  std::uint64_t limit = 7;
  std::uint64_t calls = 0;
  std::uint64_t expected_offset = 0;
  int depth = 0;
  int max_depth = 0;
  int closes = 0;
  bool valid = true;
  bool delayed = false;
  bool short_result = false;
  std::optional<io_completion<byte_buffer>> receipt;
  memory_resource* resource = nullptr;
  std::uint64_t offset = 0;
  std::uint64_t length = 0;
  std::uint64_t size() const noexcept override { return bytes; }
  std::uint64_t max_read_size() const noexcept override { return limit; }
  io_error start_read(std::uint64_t at, std::uint64_t count, memory_resource* memory,
                      io_completion<byte_buffer> completion) override {
    ++calls;
    valid = valid && at == expected_offset && count <= limit;
    expected_offset += count;
    ++depth;
    max_depth = (std::max)(depth, max_depth);
    offset = at;
    length = count;
    resource = memory;
    receipt.emplace(std::move(completion));
    if (!delayed)
      finish();
    --depth;
    return {};
  }
  void finish() {
    if (!receipt)
      throw std::logic_error{"missing receipt"};
    auto completion = std::move(*receipt);
    receipt.reset();
    auto data = make_byte_buffer(resource);
    data.resize(static_cast<std::size_t>(length - (short_result ? 1 : 0)));
    for (std::size_t index = 0; index < data.size(); ++index)
      data[index] = static_cast<std::byte>((offset + index) % 251);
    (void)completion.complete({{}, std::move(data)});
  }
  io_error start_close(io_completion<close_result> completion) override {
    ++closes;
    (void)completion.complete({{}, {}});
    return {};
  }
};
class buffer_resource final : public memory_resource {
public:
  std::size_t active = 0;
  std::size_t peak = 0;
  bool fail = false;

private:
  void* do_try_allocate(std::size_t bytes, std::size_t alignment) noexcept override {
    if (fail && bytes > 32)
      return nullptr;
    void* result = get_system_memory_resource().try_allocate(bytes, alignment);
    if (result) {
      active += bytes;
      peak = (std::max)(peak, active);
    }
    return result;
  }
  void do_deallocate(void* ptr, std::size_t bytes, std::size_t alignment) noexcept override {
    active -= bytes;
    get_system_memory_resource().deallocate(ptr, bytes, alignment);
  }
  bool do_is_equal(const memory_resource& other) const noexcept override { return this == &other; }
};
}

TEST_CASE("io - chunk aggregation respects backend limit and ordered tail", "[io][chunks]") {
  for (std::uint64_t size : {0, 98, 100}) {
    auto file = std::make_shared<pattern_file>();
    file->bytes = size;
    std::uint64_t progress = 0;
    auto request = read_all_chunked(file, {size, 40}, tasks::inline_executor(), {},
                                    [&](auto completed, auto total) {
                                      CHECK(total == size);
                                      CHECK(completed >= progress);
                                      progress = completed;
                                    });
    REQUIRE(request);
    const auto result = request->result();
    REQUIRE(result);
    REQUIRE(*result);
    REQUIRE(result->value.size() == size);
    for (std::size_t index = 0; index < size; ++index)
      CHECK(result->value[index] == static_cast<std::byte>(index % 251));
    CHECK(file->calls == (size + 6) / 7);
    CHECK(progress == size);
    CHECK(file->max_depth <= 1);
    CHECK(file->closes == 0);
  }
}

TEST_CASE("io - streaming buffer stays bounded and inline chunks do not recurse", "[io][chunks]") {
  buffer_resource memory;
  auto file = std::make_shared<pattern_file>();
  file->bytes = 20000;
  file->limit = 1;
  std::uint64_t consumed = 0;
  std::uint64_t checksum = 0;
  auto request = consume_chunks(
      file, {file->bytes, 13}, tasks::inline_executor(),
      [&](auto offset, auto block) {
        CHECK(offset == consumed);
        REQUIRE(block.size() == 1);
        CHECK(block.front() == static_cast<std::byte>(offset % 251));
        checksum += std::to_integer<unsigned char>(block.front());
        consumed += block.size();
        return io_error{};
      },
      {}, {}, &memory);
  REQUIRE(request);
  REQUIRE(request->result());
  REQUIRE(*request->result());
  CHECK(request->result()->value.bytes == consumed);
  CHECK(consumed == file->bytes);
  const auto cycles = file->bytes / 251;
  const auto remainder = file->bytes % 251;
  CHECK(checksum == cycles * (250 * 251 / 2) + remainder * (remainder - 1) / 2);
  CHECK(file->max_depth == 1);
  CHECK(memory.peak < 4096);
  CHECK(memory.active == 0);
  buffer_resource aggregate;
  auto all = read_all_chunked(file = std::make_shared<pattern_file>(), {100, 7},
                              tasks::inline_executor(), {}, {}, &aggregate);
  REQUIRE(all);
  REQUIRE(all->result());
  REQUIRE(*all->result());
  CHECK(aggregate.peak >= 100);
}

TEST_CASE("io - chunk consumption uses selected executor and applies backpressure",
          "[io][chunks]") {
  auto file = std::make_shared<pattern_file>();
  tasks::execution_context executor;
  std::uint64_t consumed = 0;
  auto request =
      consume_chunks(file, {100, 7}, executor.get_executor(), [&](auto offset, auto block) {
        CHECK(offset == consumed);
        CHECK(file->calls == consumed / 7 + 1);
        consumed += block.size();
        return io_error{};
      });
  REQUIRE(request);
  CHECK(file->calls == 1);
  CHECK(consumed == 0);
  while (!request->result())
    REQUIRE(executor.drain() >= 1);
  CHECK(consumed == 100);
  CHECK(*request->result());
  CHECK(file->closes == 0);
}

TEST_CASE("io - chunk cancellation and dropped notification produce observable terminal state",
          "[io][chunks]") {
  for (int mode : {0, 1, 2}) {
    auto file = std::make_shared<pattern_file>();
    file->delayed = mode != 2;
    tasks::execution_context executor;
    int consumed = 0;
    auto request = consume_chunks(file, {100, 7}, executor.get_executor(), [&](auto, auto) {
      ++consumed;
      return io_error{};
    });
    REQUIRE(request);
    if (mode == 0) {
      request->cancel();
      file->finish();
    } else if (mode == 1) {
      file->finish();
      request->cancel();
    }
    if (mode == 2)
      executor.clear();
    while (!request->result())
      REQUIRE(executor.drain() >= 1);
    REQUIRE_FALSE(*request->result());
    CHECK(request->result()->error().value ==
          (mode == 2 ? io_errc::completion_rejected : io_errc::cancelled));
    CHECK(file->calls == 1);
    CHECK(consumed == 0);
  }
}

TEST_CASE("io - chunk validation read failures and consumer failures stop further reads",
          "[io][chunks]") {
  auto file = std::make_shared<pattern_file>();
  CHECK(read_all_chunked(file, {100, 0}, tasks::inline_executor()).error().value ==
        io_errc::invalid_argument);
  CHECK(read_all_chunked(file, {99, 7}, tasks::inline_executor()).error().value ==
        io_errc::limit_exceeded);
  file->short_result = true;
  auto short_read = read_all_chunked(file, {100, 7}, tasks::inline_executor());
  REQUIRE(short_read);
  REQUIRE(short_read->result());
  CHECK(short_read->result()->error().value == io_errc::short_read);
  file = std::make_shared<pattern_file>();
  auto failure = consume_chunks(file, {100, 7}, tasks::inline_executor(), [](auto, auto) {
    return io_error{io_errc::backend_failure, 77, -9};
  });
  REQUIRE(failure);
  REQUIRE(failure->result());
  CHECK(failure->result()->error().backend_id == 77);
  CHECK(failure->result()->error().native_code == -9);
  CHECK(file->calls == 1);
  file = std::make_shared<pattern_file>();
  auto thrown =
      consume_chunks(file, {100, 7}, tasks::inline_executor(),
                     [](auto, auto) -> io_error { throw std::runtime_error{"consumer"}; });
  REQUIRE(thrown);
  REQUIRE(thrown->result());
  CHECK(thrown->result()->error().value == io_errc::backend_failure);
  CHECK(file->calls == 1);
}

TEST_CASE("io - chunk allocation failure and cancellation during consumption are terminal",
          "[io][chunks]") {
  buffer_resource memory;
  memory.fail = true;
  auto file = std::make_shared<pattern_file>();
  auto aggregation = read_all_chunked(file, {100, 64}, tasks::inline_executor(), {}, {}, &memory);
  CHECK(aggregation.error().value == io_errc::out_of_memory);
  CHECK(file->calls == 0);
  file->limit = 64;
  auto streaming = consume_chunks(
      file, {100, 64}, tasks::inline_executor(), [](auto, auto) { return io_error{}; }, {}, {},
      &memory);
  REQUIRE(streaming);
  REQUIRE(streaming->result());
  CHECK(streaming->result()->error().value == io_errc::out_of_memory);
  memory.fail = false;
  tasks::execution_context executor;
  file = std::make_shared<pattern_file>();
  io_request<chunk_summary>* active = nullptr;
  int consumed = 0;
  auto cancelled = consume_chunks(file, {100, 7}, executor.get_executor(), [&](auto, auto) {
    ++consumed;
    active->cancel();
    return io_error{};
  });
  REQUIRE(cancelled);
  active = &cancelled.value;
  while (!cancelled->result())
    REQUIRE(executor.drain() >= 1);
  CHECK(cancelled->result()->error().value == io_errc::cancelled);
  CHECK(consumed == 1);
  CHECK(file->calls == 1);
}

TEST_CASE("io - concurrent completion executor serializes chunk progression and cancellation",
          "[io][chunks][concurrency]") {
  for (bool cancel : {false, true}) {
    auto file = std::make_shared<pattern_file>();
    file->bytes = 10000;
    caors::tasks::thread_pool completion{2};
    std::atomic<std::uint64_t> consumed{0};
    auto request = consume_chunks(file, {file->bytes, 7}, completion.get_executor(),
                                  [&](auto offset, auto block) {
                                    if (offset != consumed.load())
                                      return io_error{io_errc::backend_failure};
                                    consumed += block.size();
                                    return io_error{};
                                  });
    REQUIRE(request);
    if (cancel)
      request->cancel();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
    while (!request->result() && std::chrono::steady_clock::now() < deadline)
      std::this_thread::yield();
    const auto result = request->result();
    REQUIRE(result);
    if (*result)
      CHECK(result->value.bytes == file->bytes);
    else
      CHECK(result->error().value == io_errc::cancelled);
    CHECK(file->valid);
    CHECK(consumed <= file->bytes);
  }
}
