// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：meta_type_registry_meta_type_global_registry_checks_parent_relationship_test.cpp

#include "caors/meta/detail/call_guard.h"
#include "caors/meta/detail/type_access.h"
#include "caors/meta/property_accessor.h"
#include "meta_type_registry_test_support.h"

#include <future>
#include <type_traits>

namespace {

struct overlay_draft_type {};
struct overlay_base_type {};
struct overlay_child_type : overlay_base_type {};
struct overlay_frozen_type {};
struct overlay_conflict_type {};
struct overlay_parallel_a {};
struct overlay_parallel_b {};
struct overlay_pinned_layer_type {};
struct overlay_pinned_call_type {};
struct overlay_detachable_call_type {};
struct overlay_frozen_policy_type {};
struct overlay_frozen_policy_late_type {};
struct overlay_explicit_freeze_type {};
struct overlay_explicit_freeze_late_type {};
struct overlay_explicit_freeze_layer_type {};
struct overlay_reflected_type {
  int value = 0;
  [[nodiscard]] int get_value() const { return value; }
};
struct overlay_detach_boundary_type {
  int value = 0;
  [[nodiscard]] int get_value() const { return value; }
};
struct overlay_owned_value {
  int* destructions = nullptr;
  ~overlay_owned_value() {
    if (destructions != nullptr) {
      ++*destructions;
    }
  }
};

} // namespace

using namespace caors::tests::meta_type_registry;

static_assert(std::is_nothrow_copy_constructible_v<caors::meta::type>);
static_assert(std::is_nothrow_move_constructible_v<caors::meta::type>);
static_assert(std::is_nothrow_copy_constructible_v<caors::meta::property>);
static_assert(std::is_nothrow_move_constructible_v<caors::meta::property>);
static_assert(std::is_nothrow_copy_constructible_v<caors::meta::method>);
static_assert(std::is_nothrow_move_constructible_v<caors::meta::method>);
static_assert(std::is_nothrow_copy_constructible_v<caors::meta::method_set>);
static_assert(std::is_nothrow_move_constructible_v<caors::meta::method_set>);
static_assert(std::is_nothrow_copy_constructible_v<caors::meta::property_accessor>);
static_assert(std::is_nothrow_move_constructible_v<caors::meta::property_accessor>);

TEST_CASE("meta type - global registry checks parent relationship", "[meta]") {
  REQUIRE(caors::registration::type_<relation_parent_type>("caors.test.type.relation_parent")
              .commit() == caors::error{});
  const auto parent = caors::meta::find("caors.test.type.relation_parent");
  REQUIRE(parent);

  REQUIRE(caors::registration::type_<relation_child_type>("caors.test.type.relation_child")
              .parent(parent.id())
              .commit() == caors::error{});
  const auto child = caors::meta::find("caors.test.type.relation_child");
  REQUIRE(child);

  CHECK(caors::meta::get_registry().contains(parent.id()));
  CHECK(caors::meta::get_registry().contains(child.id()));
  CHECK(caors::meta::is_a(child.id(), child.id()));
  CHECK(caors::meta::is_a(child.id(), parent.id()));
  CHECK_FALSE(caors::meta::is_a(parent.id(), child.id()));
  CHECK(child.parent().id() == parent.id());
}

// 原测试文件：meta_type_registry_meta_type_global_registry_finds_types_by_c_type_test.cpp

#include "meta_type_registry_test_support.h"

using namespace caors::tests::meta_type_registry;
TEST_CASE("meta type - global registry finds types by C++ type", "[meta]") {
  REQUIRE(caors::registration::type_<cpp_sample_type>("caors.test.type.cpp_sample").commit() ==
          caors::error{});

  auto type = caors::meta::find<cpp_sample_type>();
  REQUIRE(type);
  CHECK(type.name() == "caors.test.type.cpp_sample");
  CHECK(caors::meta::id("caors.test.type.cpp_sample") == type.id());
}

// 原测试文件：meta_type_registry_meta_type_global_registry_finds_types_by_stable_guid_test.cpp

#include "meta_type_registry_test_support.h"

using namespace caors::tests::meta_type_registry;
TEST_CASE("meta type - global registry finds types by stable guid", "[meta]") {
  const auto guid = caors::guid::parse("7b458821-d56a-4133-a5a6-90a8661ee474");
  REQUIRE(guid.has_value());

  REQUIRE(caors::registration::type_<guid_parent_type>("caors.test.type.guid_parent")
              .guid(caors::type_guid{*guid})
              .commit() == caors::error{});

  auto type = caors::meta::find(caors::type_guid{*guid});
  REQUIRE(type);
  CHECK(type.name() == "caors.test.type.guid_parent");
  CHECK(type.guid() == caors::type_guid{*guid});
}

// 原测试文件：meta_type_registry_meta_type_global_registry_registers_and_finds_types_test.cpp

#include "meta_type_registry_test_support.h"

using namespace caors::tests::meta_type_registry;
TEST_CASE("meta type - global registry registers and finds types", "[meta]") {
  CHECK(caors::registration::type_<registry_parent_type>("caors.test.type.registry_parent")
            .commit() == caors::error{});
  CHECK(caors::registration::type_<registry_parent_type>("caors.test.type.registry_parent")
            .commit() == meta_error(caors::errc::duplicate_id));

  auto type = caors::meta::find("caors.test.type.registry_parent");
  REQUIRE(type);
  CHECK(type.id());
  CHECK(type.name() == "caors.test.type.registry_parent");
  const caors::any value = caors::any::default_construct(type);
  REQUIRE(value.valid());
  CHECK(value.size() == sizeof(registry_parent_type));
  CHECK(value.alignment() == alignof(registry_parent_type));

  auto by_name = caors::meta::find("caors.test.type.registry_parent");
  REQUIRE(by_name);
  CHECK(by_name.id() == type.id());
}

// 原测试文件：meta_type_registry_meta_type_global_registry_reports_registration_errors_test.cpp

#include "meta_type_registry_test_support.h"

using namespace caors::tests::meta_type_registry;
TEST_CASE("meta type - global registry reports registration errors", "[meta]") {
  CHECK(caors::registration::type_<error_child_type>("caors.test.type.missing_parent_child")
            .parent(caors::type_id{std::numeric_limits<std::uint64_t>::max()})
            .commit() == meta_error(caors::errc::missing_parent));
  CHECK_FALSE(caors::meta::find<error_child_type>());

  REQUIRE(caors::registration::type_<error_parent_type>("caors.test.type.error_parent").commit() ==
          caors::error{});

  CHECK(caors::registration::type_<error_child_type>("caors.test.type.error_parent").commit() ==
        meta_error(caors::errc::duplicate_name));

  const auto guid = caors::guid::parse("c9e7a3da-2cc0-4048-97ef-d9327f71b09e");
  REQUIRE(guid.has_value());
  REQUIRE(caors::registration::type_<duplicate_guid_owner_type>("caors.test.type.guid_owner")
              .guid(caors::type_guid{*guid})
              .commit() == caors::error{});

  CHECK(caors::registration::type_<duplicate_guid_type>("caors.test.type.duplicate_guid")
            .guid(caors::type_guid{*guid})
            .commit() == meta_error(caors::errc::duplicate_guid));
  CHECK_FALSE(caors::meta::find<duplicate_guid_type>());
}

TEST_CASE("meta registry layer 隔离 draft 并在发布后冻结") {
  caors::meta::registry registry{};
  caors::meta::context base_context{registry};
  REQUIRE_FALSE(
      caors::registration::type_<overlay_base_type>(base_context, "caors.test.meta.overlay_base")
          .commit());
  auto layer = registry.create_layer();
  caors::meta::context layer_context{layer->get_registry()};
  REQUIRE(caors::meta::find<overlay_base_type>(layer_context));

  REQUIRE_FALSE(
      caors::registration::type_<overlay_draft_type>(layer_context, "caors.test.meta.overlay_draft")
          .commit());
  REQUIRE_FALSE(
      caors::registration::type_<overlay_child_type>(layer_context, "caors.test.meta.overlay_child")
          .parent<overlay_base_type>()
          .commit());
  CHECK_FALSE(caors::meta::find(base_context, "caors.test.meta.overlay_draft"));
  REQUIRE(caors::meta::find(layer_context, "caors.test.meta.overlay_draft"));

  REQUIRE_FALSE(registry.publish_layer(*layer));
  CHECK(layer->published());
  REQUIRE(caors::meta::find(base_context, "caors.test.meta.overlay_draft"));
  const auto child = caors::meta::find<overlay_child_type>(base_context);
  REQUIRE(child);
  CHECK(child.parent().id() == caors::meta::find<overlay_base_type>(base_context).id());
  CHECK_FALSE(registry.validate());
  CHECK(caors::registration::type_<overlay_frozen_type>(layer_context,
                                                        "caors.test.meta.overlay_frozen")
            .commit() == caors::error{caors::error_domain::meta, caors::errc::frozen});
}

TEST_CASE("meta registry layer 冲突失败不会污染当前 snapshot") {
  caors::meta::registry registry{};
  caors::meta::context base_context{registry};
  REQUIRE_FALSE(caors::registration::type_<overlay_draft_type>(base_context,
                                                               "caors.test.meta.overlay_conflict")
                    .commit());

  auto layer = registry.create_layer();
  caors::meta::context layer_context{layer->get_registry()};
  REQUIRE_FALSE(caors::registration::type_<overlay_conflict_type>(
                    layer_context, "caors.test.meta.overlay_conflict")
                    .commit());
  CHECK(registry.publish_layer(*layer) ==
        caors::error{caors::error_domain::meta, caors::errc::duplicate_name});
  CHECK_FALSE(layer->published());
  CHECK_FALSE(caors::meta::find<overlay_conflict_type>(base_context));
}

TEST_CASE("meta registry detach 让旧 type 失效并由 lease 保活 layer") {
  caors::meta::registry registry{};
  caors::meta::context context{registry};
  caors::meta::type stale{};
  {
    auto layer = registry.create_layer();
    caors::meta::context layer_context{layer->get_registry()};
    REQUIRE_FALSE(caors::registration::type_<overlay_draft_type>(layer_context,
                                                                 "caors.test.meta.overlay_detach")
                      .commit());
    REQUIRE_FALSE(registry.publish_layer(*layer));
    stale = caors::meta::find<overlay_draft_type>(context);
    REQUIRE(stale);

    REQUIRE_FALSE(registry.detach_layer(*layer));
    CHECK_FALSE(layer->active());
    CHECK_FALSE(caors::meta::find<overlay_draft_type>(context));
    CHECK_FALSE(stale);
    CHECK(registry.detach_layer(*layer) ==
          caors::error{caors::error_domain::meta, caors::errc::not_found});
  }

  CHECK_FALSE(stale);
}

TEST_CASE("meta registry detach 让旧 property method 和 accessor 失效") {
  caors::meta::registry registry{};
  caors::meta::context context{registry};
  caors::meta::property stale_property{};
  caors::meta::method_set stale_method_set{};
  caors::meta::method stale_method{};
  caors::meta::property_accessor stale_accessor{};
  {
    auto layer = registry.create_layer();
    caors::meta::context layer_context{layer->get_registry()};
    REQUIRE_FALSE(caors::registration::type_<overlay_reflected_type>(
                      layer_context, "caors.test.meta.overlay_reflected")
                      .meta()
                      .property("value", &overlay_reflected_type::value)
                      .method("get_value", &overlay_reflected_type::get_value)
                      .commit());
    REQUIRE_FALSE(registry.publish_layer(*layer));

    const auto reflected = caors::meta::find<overlay_reflected_type>(context);
    REQUIRE(reflected);
    stale_property = reflected.find_property("value");
    stale_method_set = reflected.find_method("get_value");
    stale_method = stale_method_set.single();
    stale_accessor = caors::meta::compile_property_accessor(stale_property);
    REQUIRE(stale_property);
    REQUIRE(stale_method_set);
    REQUIRE(stale_method);
    REQUIRE(stale_accessor);

    REQUIRE_FALSE(registry.detach_layer(*layer));
  }

  CHECK_FALSE(stale_property);
  CHECK_FALSE(stale_method_set);
  CHECK_FALSE(stale_method);
  CHECK_FALSE(stale_accessor);
}

TEST_CASE("meta registry detach 后旧 view 查询和调用边界稳定") {
  caors::meta::registry registry{};
  caors::meta::context context{registry};
  auto layer = registry.create_layer();
  caors::meta::context layer_context{layer->get_registry()};

  REQUIRE_FALSE(caors::registration::type_<overlay_detach_boundary_type>(
                    layer_context, "caors.test.meta.overlay_detach_boundary")
                    .meta()
                    .property("value", &overlay_detach_boundary_type::value)
                    .method("get_value", &overlay_detach_boundary_type::get_value)
                    .commit());
  REQUIRE_FALSE(registry.publish_layer(*layer));

  caors::meta::type stale_type = caors::meta::find<overlay_detach_boundary_type>(context);
  REQUIRE(stale_type);
  caors::meta::property stale_property = stale_type.find_property("value");
  caors::meta::method_set stale_method_set = stale_type.find_method("get_value");
  caors::meta::method stale_method = stale_method_set.single();
  caors::meta::property_accessor stale_accessor =
      caors::meta::compile_property_accessor(stale_property);
  REQUIRE(stale_property);
  REQUIRE(stale_method_set);
  REQUIRE(stale_method);
  REQUIRE(stale_accessor);

  overlay_detach_boundary_type object{7};
  const caors::meta::instance instance = caors::meta::view(object);
  REQUIRE(stale_property.try_get<int>(instance) == 7);
  REQUIRE(stale_method.invoke_result_of(instance));

  REQUIRE_FALSE(registry.detach_layer(*layer));
  CHECK_FALSE(layer->active());
  CHECK(layer->in_flight_calls() == 0);

  CHECK_FALSE(caors::meta::find<overlay_detach_boundary_type>(context));
  CHECK_FALSE(stale_type);
  CHECK_FALSE(stale_type.id());
  CHECK_FALSE(stale_type.name());
  CHECK(stale_type.property_count() == 0);
  CHECK(stale_type.method_count() == 0);
  CHECK_FALSE(stale_type.find_property("value"));
  CHECK_FALSE(stale_type.find_method("get_value"));

  CHECK_FALSE(stale_property);
  CHECK_FALSE(stale_property.name());
  CHECK_FALSE(stale_property.get_owner_type());
  CHECK_FALSE(stale_property.get_type());
  CHECK_FALSE(stale_property.get_ref(instance));
  CHECK_FALSE(stale_property.get_any(instance));
  CHECK_FALSE(stale_property.try_set(instance, 42));
  CHECK_FALSE(stale_accessor);
  CHECK_FALSE(stale_accessor.get_ref(&object));

  CHECK_FALSE(stale_method_set);
  CHECK_FALSE(stale_method_set.name());
  CHECK(stale_method_set.overload_count() == 0);
  CHECK_FALSE(stale_method_set.single());
  CHECK_FALSE(stale_method_set.resolve());
  CHECK_FALSE(stale_method_set.invoke_result_of(instance));
  CHECK(stale_method_set.invoke_result_of(instance).failure_reason() ==
        caors::meta::invoke_failure_reason::invalid_method);

  CHECK_FALSE(stale_method);
  CHECK_FALSE(stale_method.name());
  CHECK_FALSE(stale_method.get_owner_type());
  CHECK_FALSE(stale_method.get_return_type());
  CHECK(stale_method.parameter_count() == 0);
  CHECK_FALSE(stale_method.invoke_result_of(instance));
  CHECK(stale_method.invoke_result_of(instance).failure_reason() ==
        caors::meta::invoke_failure_reason::invalid_method);
  CHECK(layer->in_flight_calls() == 0);
}

TEST_CASE("meta registry detach 后旧 any 拒绝复制但仍安全析构") {
  caors::meta::registry registry{};
  caors::meta::context context{registry};
  auto layer = registry.create_layer();
  caors::meta::context layer_context{layer->get_registry()};
  REQUIRE_FALSE(caors::registration::type_<overlay_owned_value>(
                    layer_context, "caors.test.meta.overlay_owned_value")
                    .commit());
  REQUIRE_FALSE(registry.publish_layer(*layer));

  const auto reflected = caors::meta::find<overlay_owned_value>(context);
  REQUIRE(reflected);
  int destructions = 0;
  overlay_owned_value source{&destructions};
  auto stale = caors::meta::any::copy_from_raw(reflected, &source);
  REQUIRE(stale);

  REQUIRE_FALSE(registry.detach_layer(*layer));
  CHECK_FALSE(stale);
  CHECK_FALSE(caors::meta::any{stale});
  stale.reset();
  CHECK(destructions == 1);
}

TEST_CASE("meta registry 并发发布 layer 不会丢失已发布 snapshot") {
  caors::meta::registry registry{};
  caors::meta::context base_context{registry};
  auto first = registry.create_layer();
  auto second = registry.create_layer();
  caors::meta::context first_context{first->get_registry()};
  caors::meta::context second_context{second->get_registry()};
  REQUIRE_FALSE(caors::registration::type_<overlay_parallel_a>(first_context,
                                                               "caors.test.meta.overlay_parallel_a")
                    .commit());
  REQUIRE_FALSE(caors::registration::type_<overlay_parallel_b>(second_context,
                                                               "caors.test.meta.overlay_parallel_b")
                    .commit());

  auto first_publish =
      std::async(std::launch::async, [&] { return registry.publish_layer(*first); });
  auto second_publish =
      std::async(std::launch::async, [&] { return registry.publish_layer(*second); });
  CHECK_FALSE(first_publish.get());
  CHECK_FALSE(second_publish.get());
  CHECK(caors::meta::find<overlay_parallel_a>(base_context));
  CHECK(caors::meta::find<overlay_parallel_b>(base_context));
}

TEST_CASE("meta registry 管理策略保持创建选项") {
  caors::meta::registry default_registry{};
  CHECK(default_registry.management_policy() ==
        caors::meta::registry_management_policy::locked_append_only);

  caors::meta::registry manual_registry{caors::meta::registry_options{
      caors::meta::registry_management_policy::manual_synchronization}};
  CHECK(manual_registry.management_policy() ==
        caors::meta::registry_management_policy::manual_synchronization);
}

TEST_CASE("meta registry frozen_read_optimized 在 init 后冻结 primary layer") {
  caors::meta::registry registry{caors::meta::registry_options{
      caors::meta::registry_management_policy::frozen_read_optimized}};
  caors::meta::context context{registry};

  REQUIRE_FALSE(caors::registration::type_<overlay_frozen_policy_type>(
                    context, "caors.test.meta.frozen_policy")
                    .commit());
  REQUIRE_FALSE(registry.init());

  CHECK(caors::meta::find<overlay_frozen_policy_type>(context));
  CHECK(registry.size() == 1);
  CHECK(caors::registration::type_<overlay_frozen_policy_late_type>(
            context, "caors.test.meta.frozen_policy_late")
            .commit() == caors::error{caors::error_domain::meta, caors::errc::frozen});
}

TEST_CASE("meta registry 显式 freeze 冻结 primary 但允许发布和 detach layer") {
  caors::meta::registry registry{};
  caors::meta::context context{registry};

  REQUIRE_FALSE(caors::registration::type_<overlay_explicit_freeze_type>(
                    context, "caors.test.meta.explicit_freeze")
                    .commit());
  REQUIRE_FALSE(registry.freeze());
  CHECK(registry.frozen());
  CHECK_FALSE(registry.freeze());
  CHECK(caors::meta::find<overlay_explicit_freeze_type>(context));
  CHECK(caors::registration::type_<overlay_explicit_freeze_late_type>(
            context, "caors.test.meta.explicit_freeze_late")
            .commit() == caors::error{caors::error_domain::meta, caors::errc::frozen});

  auto layer = registry.create_layer();
  caors::meta::context layer_context{layer->get_registry()};
  REQUIRE_FALSE(caors::registration::type_<overlay_explicit_freeze_layer_type>(
                    layer_context, "caors.test.meta.explicit_freeze_layer")
                    .commit());
  REQUIRE_FALSE(registry.publish_layer(*layer));
  CHECK(caors::meta::find<overlay_explicit_freeze_layer_type>(context));
  CHECK(registry.size() == 2);

  REQUIRE_FALSE(registry.detach_layer(*layer));
  CHECK_FALSE(caors::meta::find<overlay_explicit_freeze_layer_type>(context));
  CHECK(registry.size() == 1);
  CHECK(registry.frozen());
}

TEST_CASE("meta registry layer lifetime 控制 detach 和 call guard") {
  caors::meta::registry registry{};
  caors::meta::context context{registry};

  auto pinned = registry.create_layer(caors::meta::registry_layer_lifetime::pinned_process);
  caors::meta::context pinned_context{pinned->get_registry()};
  REQUIRE_FALSE(caors::registration::type_<overlay_pinned_layer_type>(
                    pinned_context, "caors.test.meta.overlay_pinned_layer")
                    .commit());
  REQUIRE_FALSE(registry.publish_layer(*pinned));
  CHECK(pinned->lifetime() == caors::meta::registry_layer_lifetime::pinned_process);
  CHECK(registry.detach_layer(*pinned) ==
        caors::error{caors::error_domain::meta, caors::errc::unsupported});
  CHECK(pinned->active());
  CHECK(caors::meta::find<overlay_pinned_layer_type>(context));

  auto pinned_call = registry.create_layer(caors::meta::registry_layer_lifetime::pinned_process);
  caors::meta::context pinned_call_context{pinned_call->get_registry()};
  REQUIRE_FALSE(caors::registration::type_<overlay_pinned_call_type>(
                    pinned_call_context, "caors.test.meta.overlay_pinned_call")
                    .commit());
  REQUIRE_FALSE(registry.publish_layer(*pinned_call));
  const auto pinned_type = caors::meta::find<overlay_pinned_call_type>(context);
  REQUIRE(pinned_type);
  {
    const caors::meta::detail::layer_call_guard guard{
        caors::meta::detail::type_access::layer_of(pinned_type)};
    REQUIRE(guard);
    CHECK(pinned_call->in_flight_calls() == 0);
  }
  CHECK(pinned_call->in_flight_calls() == 0);

  auto detachable = registry.create_layer();
  caors::meta::context detachable_context{detachable->get_registry()};
  REQUIRE_FALSE(caors::registration::type_<overlay_detachable_call_type>(
                    detachable_context, "caors.test.meta.overlay_detachable_call")
                    .commit());
  REQUIRE_FALSE(registry.publish_layer(*detachable));
  const auto detachable_type = caors::meta::find<overlay_detachable_call_type>(context);
  REQUIRE(detachable_type);
  CHECK(detachable->lifetime() == caors::meta::registry_layer_lifetime::detachable);
  {
    const caors::meta::detail::layer_call_guard guard{
        caors::meta::detail::type_access::layer_of(detachable_type)};
    REQUIRE(guard);
    CHECK(detachable->in_flight_calls() == 1);
  }
  CHECK(detachable->in_flight_calls() == 0);
}
