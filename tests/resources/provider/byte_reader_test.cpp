// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/memory_file_system.h"
#include "caors/resources/byte_reader.h"
#include "caors/tasks/execution_context.h"
#include <catch2/catch_all.hpp>

namespace {
class host_reader final : public caors::resources::byte_reader {
public:
  caors::resources::read_completion pending;
  bool refuse = false;
  caors::io::io_error read(caors::resources::read_context,
                           caors::resources::read_completion completion) override {
    if (refuse)
      return {caors::io::io_errc::unsupported, 42, 7};
    pending = std::move(completion);
    return {};
  }
};
}
TEST_CASE("resources - byte reader accepts host delayed completion and refusal",
          "[resources][byte-reader]") {
  caors::tasks::execution_context completion;
  host_reader reader;
  int calls = 0;
  auto request = caors::resources::request_bytes(reader, {}, completion.get_executor(),
                                                 [&](const auto& value) {
                                                   REQUIRE(value);
                                                   CHECK(value.value.size() == 3);
                                                   ++calls;
                                                 });
  REQUIRE(request);
  CHECK_FALSE(request.value.result());
  auto bytes = caors::io::make_byte_buffer();
  bytes.resize(3);
  CHECK(reader.pending.complete({{}, std::move(bytes)}));
  CHECK_FALSE(reader.pending.complete({{}, {}}));
  CHECK(calls == 0);
  (void)completion.drain();
  REQUIRE(request.value.result());
  CHECK(calls == 1);
  reader.refuse = true;
  auto refused = caors::resources::request_bytes(reader, {}, completion.get_executor(),
                                                 [&](const auto&) { ++calls; });
  CHECK_FALSE(refused);
  CHECK(refused.error().backend_id == 42);
  CHECK(refused.error().native_code == 7);
  (void)completion.drain();
  CHECK(calls == 1);
}
TEST_CASE("resources - abandoned and discarded byte completions terminate",
          "[resources][byte-reader]") {
  caors::tasks::execution_context completion;
  host_reader reader;
  auto dropped = caors::resources::request_bytes(reader, {}, completion.get_executor());
  REQUIRE(dropped);
  reader.pending = {};
  (void)completion.drain();
  REQUIRE(dropped.value.result());
  CHECK(dropped.value.result()->error().value == caors::io::io_errc::cancelled);
  auto rejected = caors::resources::request_bytes(reader, {}, completion.get_executor());
  REQUIRE(rejected);
  CHECK(reader.pending.complete({{}, {}}));
  completion.clear();
  REQUIRE(rejected.value.result());
  CHECK(rejected.value.result()->error().value == caors::io::io_errc::completion_rejected);
}
TEST_CASE("resources - file byte reader handles inline callbacks", "[resources][byte-reader]") {
  auto files = std::make_shared<caors::io::memory_file_system>();
  REQUIRE_FALSE(files->write_all("data", "abc"));
  caors::resources::file_byte_reader reader;
  auto request = caors::resources::request_bytes(
      reader, {{files, caors::io::path{"data"}, 0}, caors::tasks::inline_executor(), 3, nullptr},
      caors::tasks::inline_executor());
  REQUIRE(request);
  REQUIRE(request.value.result());
  REQUIRE(*request.value.result());
  CHECK(request.value.result()->value.size() == 3);
  CHECK(files.use_count() == 1);
}
