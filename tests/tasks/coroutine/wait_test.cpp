// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory/tracking.h"
#include "caors/tasks/asio.h"
#include "caors/tasks/execution_context.h"
#include "caors/tasks/inline_executor.h"

#include <catch2/catch_all.hpp>

namespace {

caors::tasks::async_operation wait_and_record(caors::tasks::request source,
                                              caors::error* observed) {
  *observed = co_await caors::tasks::async_wait(std::move(source));
  co_return caors::error{};
}

} // namespace

TEST_CASE("tasks - multiple coroutines await one shared request", "[tasks][asio][coroutine]") {
  caors::tasks::execution_context source_context;
  caors::tasks::executor start = caors::tasks::inline_executor();
  caors::tasks::request source =
      caors::tasks::submit_request(source_context.get_executor(), caors::tasks::task{[] {}});
  caors::error first_status{caors::error_domain::core, caors::errc::pending};
  caors::error second_status{caors::error_domain::core, caors::errc::pending};

  caors::tasks::request first = caors::tasks::spawn(start, wait_and_record(source, &first_status));
  caors::tasks::request second =
      caors::tasks::spawn(start, wait_and_record(source, &second_status));
  CHECK(first.status() == caors::tasks::request_status::running);
  CHECK(second.status() == caors::tasks::request_status::running);

  CHECK(source_context.poll() == 1);
  CHECK(source.completed());
  CHECK(first.completed());
  CHECK(second.completed());
  CHECK_FALSE(first_status);
  CHECK_FALSE(second_status);
}

TEST_CASE("tasks - async wait preserves source failure", "[tasks][asio][coroutine]") {
  caors::tasks::execution_context source_context;
  caors::tasks::executor start = caors::tasks::inline_executor();
  caors::tasks::request source = caors::tasks::submit_request(
      source_context.get_executor(), caors::tasks::task{[] {
        return caors::error{caors::error_domain::core, caors::errc::unsupported};
      }});
  caors::error observed;
  caors::tasks::request waiting = caors::tasks::spawn(start, wait_and_record(source, &observed));

  CHECK(source_context.poll() == 1);
  CHECK(source.failed());
  CHECK(waiting.completed());
  CHECK(observed == caors::error{caors::error_domain::core, caors::errc::unsupported});
}

TEST_CASE("tasks - async wait resumes when source is cancelled", "[tasks][asio][coroutine]") {
  caors::tasks::executor start = caors::tasks::inline_executor();
  caors::tasks::request source = caors::tasks::request::make();
  caors::error observed;
  caors::tasks::request waiting = caors::tasks::spawn(start, wait_and_record(source, &observed));

  REQUIRE(source.cancel());
  CHECK(waiting.completed());
  CHECK(observed == caors::error{caors::error_domain::core, caors::errc::cancelled});
}

TEST_CASE("tasks - async wait does not suspend for terminal request", "[tasks][asio][coroutine]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::tasks::executor start = caors::tasks::inline_executor();
  caors::tasks::request source = caors::tasks::request::make_completed(&tracking);
  const caors::memory_statistics baseline = tracking.statistics();
  caors::error observed{caors::error_domain::core, caors::errc::pending};
  caors::tasks::request waiting = caors::tasks::spawn(start, wait_and_record(source, &observed));

  CHECK(waiting.completed());
  CHECK_FALSE(observed);
  CHECK(tracking.statistics().allocation_count == baseline.allocation_count);
}

TEST_CASE("tasks - async wait state uses request long lived resource", "[tasks][asio][coroutine]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::tasks::executor start = caors::tasks::inline_executor();
  caors::tasks::request source = caors::tasks::request::make(&tracking);
  const std::size_t baseline = tracking.statistics().bytes_in_use;
  caors::error observed;

  caors::tasks::request waiting = caors::tasks::spawn(start, wait_and_record(source, &observed));
  CHECK(tracking.statistics().bytes_in_use > baseline);

  REQUIRE(source.cancel());
  CHECK(waiting.completed());
  CHECK(tracking.statistics().bytes_in_use == baseline);
}
