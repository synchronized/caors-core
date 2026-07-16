// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

namespace {

class reload_notified_resource : public caors::resource {
public:
  using caors::resource::resource;

  int width = 0;
};

class reload_notified_loader final : public caors::resources::loader<reload_notified_resource> {
public:
  [[nodiscard]] caors::resources::load_result<reload_notified_resource>
  load(caors::resource_guid guid, caors::memory_resource* resource = nullptr) override {
    ++load_count;
    if (status.failed()) {
      return {status, {}};
    }

    auto loaded = caors::memory::make_unique<reload_notified_resource>(resource);
    loaded->set_guid(guid);
    loaded->width = width;
    return {{}, std::move(loaded)};
  }

  int width = 0;
  int load_count = 0;
  caors::error status{};
};

} // namespace

TEST_CASE("resources - reload_and_notify emits success events", "[resources]") {
  caors::resource_cache<reload_notified_resource> cache;
  reload_notified_resource* initial = cache.create(test_guid_a);
  REQUIRE(initial != nullptr);
  initial->width = 128;

  reload_notified_loader loader;
  loader.width = 256;

  caors::resources::reload_signal<reload_notified_resource> reloaded;
  caors::resources::reload_event<reload_notified_resource> captured{};
  int notify_count = 0;
  auto listener = [&](const caors::resources::reload_event<reload_notified_resource>& event) {
    ++notify_count;
    captured = event;
  };
  [[maybe_unused]] caors::event_connection connection = reloaded.connect(listener);

  caors::resources::handle_result<reload_notified_resource> result =
      caors::resources::reload_and_notify(cache, loader, test_guid_a, reloaded);

  REQUIRE(result.succeeded());
  REQUIRE(result.value);
  CHECK(notify_count == 1);
  CHECK(captured.succeeded());
  CHECK(captured.guid == test_guid_a);
  CHECK(captured.resource == result.value.get());
  CHECK(captured.resource == cache.find(test_guid_a));
  CHECK(captured.resource->width == 256);
}

TEST_CASE("resources - reload_and_notify emits failure events and keeps old resources",
          "[resources]") {
  caors::resource_cache<reload_notified_resource> cache;
  reload_notified_resource* initial = cache.create(test_guid_a);
  REQUIRE(initial != nullptr);
  initial->width = 128;

  reload_notified_loader loader;
  loader.status = {caors::error_domain::resources, caors::errc::backend_failure};

  caors::resources::reload_signal<reload_notified_resource> reloaded;
  caors::resources::reload_event<reload_notified_resource> captured{};
  int notify_count = 0;
  auto listener = [&](const caors::resources::reload_event<reload_notified_resource>& event) {
    ++notify_count;
    captured = event;
  };
  [[maybe_unused]] caors::event_connection connection = reloaded.connect(listener);

  caors::resources::handle_result<reload_notified_resource> result =
      caors::resources::reload_and_notify(cache, loader, test_guid_a, reloaded);

  CHECK(result.failed());
  CHECK(result.status ==
        caors::error{caors::error_domain::resources, caors::errc::backend_failure});
  CHECK(notify_count == 1);
  CHECK(captured.failed());
  CHECK(captured.status == result.status);
  CHECK(captured.guid == test_guid_a);
  CHECK(captured.resource == nullptr);
  CHECK(cache.find(test_guid_a) == initial);
  CHECK(initial->width == 128);
}

TEST_CASE("resources - reload_and_notify respects disconnected listeners", "[resources]") {
  caors::resource_cache<reload_notified_resource> cache;
  reload_notified_resource* initial = cache.create(test_guid_a);
  REQUIRE(initial != nullptr);

  reload_notified_loader loader;
  loader.width = 512;

  caors::resources::reload_signal<reload_notified_resource> reloaded;
  int notify_count = 0;
  auto listener = [&](const caors::resources::reload_event<reload_notified_resource>&) {
    ++notify_count;
  };
  caors::event_connection connection = reloaded.connect(listener);
  REQUIRE(connection.disconnect());

  caors::resources::handle_result<reload_notified_resource> result =
      caors::resources::reload_and_notify(cache, loader, test_guid_a, reloaded);

  REQUIRE(result.succeeded());
  CHECK(notify_count == 0);
  CHECK(cache.find(test_guid_a)->width == 512);
}
