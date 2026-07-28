// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/events/signal.h"
#include "caors/plugins/registration_scope.h"
#include "caors/tasks.h"

#include <catch2/catch_all.hpp>

namespace {

constexpr std::string_view service_id_text = "a331397a-60fd-43cc-8202-d772e3587373";

class holding_executor final : public caors::tasks::executor {
public:
  [[nodiscard]] caors::error submit(caors::tasks::task next) override {
    pending = std::move(next);
    return {};
  }

  caors::tasks::task pending{};
};

} // namespace

TEST_CASE("插件 registration scope 清理服务和事件") {
  caors::plugins::plugin_host host{};
  int owner = 0;
  caors::plugins::plugin_registration_scope scope{host, &owner};
  const caors::guid service_id = *caors::guid::parse(service_id_text);
  constexpr int service = 42;
  REQUIRE_FALSE(scope.publish_service(service_id, 1, &service));

  caors::events::signal<void()> changed{};
  int calls = 0;
  REQUIRE_FALSE(scope.track(changed.connect([&] { ++calls; })));
  changed.emit();
  CHECK(calls == 1);

  const auto result = scope.cleanup();
  CHECK(result.can_unload());
  CHECK(result.removed_services == 1);
  CHECK(result.disconnected_events == 1);
  CHECK(host.find_service(service_id, 1, 1) == nullptr);
  changed.emit();
  CHECK(calls == 1);
  CHECK(scope.cleanup().removed_services == 1);
}

TEST_CASE("插件 registration scope 取消请求但不错误宣称任务可卸载") {
  caors::plugins::plugin_host host{};
  int owner = 0;
  caors::plugins::plugin_registration_scope scope{host, &owner};
  holding_executor executor{};
  auto request = caors::tasks::submit_request(executor, caors::tasks::task{[] {}});
  REQUIRE_FALSE(scope.track(request));

  const auto result = scope.cleanup();
  CHECK_FALSE(result.can_unload());
  CHECK(result.status == caors::error{caors::error_domain::plugins, caors::errc::unsupported});
  CHECK(result.cancelled_requests == 1);
  CHECK(caors::plugins::has_blocker(result.blockers,
                                    caors::plugins::plugin_unload_blocker::pending_task));
}

TEST_CASE("插件擦除值提供结构化卸载阻止原因") {
  caors::plugins::plugin_host host{};
  int owner = 0;
  caors::plugins::plugin_registration_scope scope{host, &owner};
  scope.record_plugin_owned_value(2);
  scope.release_plugin_owned_value();

  const auto result = scope.cleanup();
  CHECK_FALSE(result.can_unload());
  CHECK(caors::plugins::has_blocker(result.blockers,
                                    caors::plugins::plugin_unload_blocker::plugin_owned_value));
  CHECK_FALSE(caors::plugins::has_blocker(result.blockers,
                                          caors::plugins::plugin_unload_blocker::pinned_process));
}
