// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/plugin_binary.h"
#include "caors/plugins/plugin_manager.h"

#include "caors/meta/capability_facade.h"
#include "caors/meta/extension_view.h"
#include "caors/meta/instance.h"
#include "caors/meta/property.h"
#include "caors/meta/property_accessor.h"
#include "caors/meta/registry.h"
#include "caors/meta/value/any.h"
#include "caors/meta/value/associative_view.h"
#include "caors/meta/value/capabilities.h"
#include "caors/meta/value/enum_view.h"
#include "caors/meta/value/indirect_view.h"
#include "caors/meta/value/pair_view.h"
#include "caors/meta/value/sequence_view.h"
#include "caors/meta/value/set_view.h"
#include "caors/meta/value/tuple_view.h"
#include "caors/meta/value/variant_view.h"
#include "caors/object.h"
#include "caors/plugins/plugin_host.h"
#include "caors/serialization/codec.h"
#include "caors/serialization/type_schema.h"
#include "plugin_protocol_test_extension.h"

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <string>
#include <utility>

namespace {

[[nodiscard]] int fail(const char* message) {
  std::cerr << message << '\n';
  return 1;
}

void CAORS_PLUGIN_CALL capture_log(void* context, std::uint32_t, const char* message,
                                   std::size_t size) {
  static_cast<std::string*>(context)->assign(message, size);
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 5) {
    return fail("插件协议测试需要四个动态库路径");
  }

  caors::plugins::plugin_host host{};
  std::string log_message{};
  host.set_log(&capture_log, &log_message);
  auto good_result = caors::plugins::plugin_binary::load(argv[1], &host.abi());
  if (!good_result) {
    return fail("无法加载有效插件");
  }
  auto plugin = std::move(*good_result);
  const std::filesystem::path plugin_path = plugin.path();
  caors::plugins::plugin_library_lease library_lease = plugin.library_lease();
  if (!plugin.valid() || plugin.state() != caors::plugins::plugin_state::loaded || !library_lease ||
      library_lease.path() != plugin_path || library_lease.use_count() < 2 ||
      plugin.descriptor().name != "caors.test.good" ||
      plugin.descriptor().display_name != "CAORS 测试插件" ||
      plugin.descriptor().plugin_version != caors::version{1, 2, 3} ||
      log_message != "插件已协商宿主扩展") {
    return fail("有效插件描述信息不正确");
  }

  if (plugin.start().value() != caors::errc::invalid_argument || plugin.initialize() ||
      plugin.state() != caors::plugins::plugin_state::initialized || plugin.start() ||
      plugin.state() != caors::plugins::plugin_state::running) {
    return fail("插件初始化或启动状态转换失败");
  }

  const auto* answer = static_cast<const int*>(plugin.get_extension("caors.test.answer", 1));
  if (answer == nullptr || *answer != 42 ||
      plugin.get_extension("caors.test.answer", 2) != nullptr) {
    return fail("插件 extension 查询失败");
  }

  if (plugin.shutdown().value() != caors::errc::invalid_argument || plugin.stop() ||
      plugin.state() != caors::plugins::plugin_state::stopped || plugin.shutdown() ||
      plugin.state() != caors::plugins::plugin_state::loaded) {
    return fail("插件停止或关闭状态转换失败");
  }

  caors::meta::registry registry{};
  auto layer = registry.create_layer();
  if (library_lease.bind(*layer)) {
    return fail("无法将 library lease 绑定到 registry layer");
  }
  plugin = {};
  if (!library_lease || library_lease.path() != plugin_path || library_lease.use_count() != 2) {
    return fail("插件二进制释放后 registry layer 没有继续保持动态库");
  }
  layer.reset();
  if (library_lease.use_count() != 1) {
    return fail("registry layer 释放后没有归还动态库租约");
  }

  const auto bad_abi = caors::plugins::plugin_binary::load(argv[2]);
  if (bad_abi || bad_abi.error() !=
                     caors::error{caors::error_domain::plugins, caors::errc::inconsistent_type}) {
    return fail("不兼容插件 ABI 没有被拒绝");
  }

  const auto missing_entry = caors::plugins::plugin_binary::load(argv[3]);
  if (missing_entry ||
      missing_entry.error() != caors::error{caors::error_domain::plugins, caors::errc::not_found}) {
    return fail("缺少入口的动态库没有被拒绝");
  }

  auto failure_result = caors::plugins::plugin_binary::load(argv[4]);
  if (!failure_result) {
    return fail("无法加载生命周期失败插件");
  }
  auto failure = std::move(*failure_result);
  if (!failure.initialize() || failure.state() != caors::plugins::plugin_state::failed ||
      failure.shutdown() || failure.state() != caors::plugins::plugin_state::loaded) {
    return fail("插件生命周期失败没有进入 failed 状态");
  }

  caors::plugins::plugin_manager manager{registry};
  auto managed_result = manager.load(argv[1]);
  if (!managed_result) {
    return fail("plugin manager 无法加载有效插件");
  }
  const caors::plugins::plugin managed = *managed_result;
  const caors::guid managed_id = managed.id();
  const std::uint64_t first_generation = managed.generation();
  if (!managed || manager.size() != 1 ||
      manager.find(managed_id).generation() != first_generation ||
      manager.load(argv[1]).error() !=
          caors::error{caors::error_domain::plugins, caors::errc::duplicate_id}) {
    return fail("plugin manager 索引或重复 ID 校验错误");
  }
  if (manager.initialize(managed_id) ||
      managed.state() != caors::plugins::plugin_state::initialized) {
    return fail("plugin manager 初始化或 layer 发布失败");
  }
  auto reflected = registry.find_type("caors.test.plugin.reflected");
  const caors::type_id reflected_id = reflected.id();
  auto reflected_property = reflected.find_property("value");
  auto reflected_accessor = caors::meta::compile_property_accessor(reflected_property);
  auto reflected_method = reflected.find_method("block");
  auto reflected_overload = reflected_method.single();
  auto reflected_nested = reflected.find_nested("nested");
  const auto* reflected_capability = reflected.get_capability<caors_plugin_protocol_capability>();
  auto owned = caors::any::default_construct(reflected);
  caors::meta::instance object{owned.ref()};
  caors::objects::object_view object_facade{owned.ref()};
  auto sequence = object.get_ref("values").get_capability<caors::meta::sequence_capability>();
  auto set = object.get_ref("tags").get_capability<caors::meta::set_capability>();
  auto associative = object.get_ref("lookup").get_capability<caors::meta::associative_capability>();
  auto pair = object.get_ref("pair").get_capability<caors::meta::pair_capability>();
  auto tuple = object.get_ref("tuple").get_capability<caors::meta::tuple_capability>();
  auto variant = object.get_ref("variant").get_capability<caors::meta::variant_capability>();
  auto indirect = object.get_ref("optional").get_capability<caors::meta::indirect_capability>();
  auto pointer = object.get_ref("object_pointer").get_capability<caors::objects::ptr_capability>();
  auto handle = object.get_ref("object_handle").get_capability<caors::objects::handle_capability>();
  auto enum_value = object.get_ref("enum").get_capability<caors::meta::enum_capability>();
  auto schema = reflected.get_capability<caors::serialization::schema_view>();
  auto proxy_capability = reflected.get_capability<caors_plugin_protocol_proxy_capability>();
  auto proxy_view = owned.ref().get_capability<caors_plugin_protocol_proxy_capability>();
  auto reflected_object = registry.find_type("caors.test.plugin.object");
  auto object_factory =
      reflected_object.get_capability<caors::objects::object_factory_capability>();
  caors::object* created_object = object_factory.try_create();
  const caors::object_handle created_handle{
      created_object != nullptr ? created_object->id() : caors::object_id{},
      created_object != nullptr ? created_object->type_id() : caors::type_id{}};
  const bool pointer_round_trip = created_object != nullptr && pointer.set(created_object) &&
                                  pointer.get() == created_object && pointer.set(nullptr) &&
                                  pointer.get() == nullptr;
  const bool handle_round_trip =
      created_object != nullptr && handle.set(created_handle) && handle.get() == created_handle;
  const bool object_destroyed = created_object != nullptr && object_factory.destroy(created_object);
  auto extensions = registry.extension_view<caors_plugin_protocol_meta_extension>();
  const auto meta_extension = extensions.find<caors_plugin_protocol_meta_extension>(reflected_id);
  auto factory = reflected.get_capability<caors::meta::factory_capability>();
  auto factory_owned = factory.construct();
  auto constructor = reflected.get_capability<caors::meta::constructor_capability>();
  int constructor_input = 88;
  std::array<caors::argument, 1> constructor_arguments{caors::arg(constructor_input)};
  auto constructor_owned = constructor.construct(
      std::span<const caors::argument>{constructor_arguments.data(), constructor_arguments.size()});
  const int* constructed_value =
      reflected_property.get_ref(caors::meta::instance{constructor_owned.ref()}).try_get<int>();
  auto codec = caors::serialization::find_codec(reflected);
  auto codec_view = codec.bind(owned.ref());
  const auto codec_value = codec_view.make_value(caors::serialization::context{});
  const auto* control =
      static_cast<const caors_plugin_protocol_test_extension*>(managed.get_extension(
          CAORS_PLUGIN_PROTOCOL_TEST_EXTENSION_ID, CAORS_PLUGIN_PROTOCOL_TEST_EXTENSION_VERSION));
  if (!reflected || !reflected_property || !reflected_accessor || !reflected_method ||
      !reflected_overload || reflected_method.overload_count() != 1 || !reflected_nested ||
      reflected_nested.name() != "caors.test.plugin.nested" || reflected_capability == nullptr ||
      reflected_capability->value != 73 || !owned || !object || !object_facade ||
      !object_facade.find_property("value") || !sequence ||
      sequence.size() != std::optional<std::size_t>{3} || !factory || !factory_owned || !set ||
      set.size() != std::optional<std::size_t>{2} || !associative ||
      associative.size() != std::optional<std::size_t>{1} || !pair ||
      pair.first_ref().try_get<int>() == nullptr || *pair.first_ref().try_get<int>() != 7 ||
      !tuple || tuple.size() != 2 || tuple.element_ref(1).try_get<int>() == nullptr ||
      *tuple.element_ref(1).try_get<int>() != 80 || !variant ||
      variant.index() != std::optional<std::size_t>{0} ||
      variant.value_ref().try_get<int>() == nullptr || *variant.value_ref().try_get<int>() != 9 ||
      !indirect || !indirect.has_value() || indirect.value_ref().try_get<int>() == nullptr ||
      *indirect.value_ref().try_get<int>() != 10 || !pointer || !handle || !reflected_object ||
      !object_factory || !object_factory.constructible() || !pointer_round_trip ||
      !handle_round_trip || !object_destroyed || !enum_value ||
      enum_value.to_i64() != std::optional<std::int64_t>{12} || !schema ||
      schema.field_count() != 1 || !schema.has_explicit_fields() || !proxy_capability ||
      !proxy_view || proxy_view.proxy_value() != 42 || !extensions || extensions.size() != 1 ||
      !meta_extension || meta_extension->value != 91 || !codec || !constructor ||
      constructor.argument_count() != 1 || !constructor_owned || constructed_value == nullptr ||
      *constructed_value != 88 || !codec.can_make_value() || !codec_view || !codec_value ||
      control == nullptr || control->struct_size < sizeof(caors_plugin_protocol_test_extension) ||
      manager.start(managed_id) || managed.state() != caors::plugins::plugin_state::running) {
    return fail("插件反射类型、method 或测试控制扩展没有发布");
  }
  control->reset(control->context);
  auto invocation = std::async(std::launch::async, [reflected_method, object] {
    return reflected_method.invoke_result_of(object);
  });
  const auto enter_deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
  while (control->entered(control->context) == 0 &&
         std::chrono::steady_clock::now() < enter_deadline) {
    std::this_thread::yield();
  }
  if (control->entered(control->context) == 0) {
    control->release(control->context);
    (void)invocation.get();
    return fail("插件 method 没有进入 in-flight 状态");
  }
  if (manager.unload(managed_id) !=
          caors::error{caors::error_domain::plugins, caors::errc::pending} ||
      managed.state() != caors::plugins::plugin_state::detached_pending_release ||
      registry.find_type("caors.test.plugin.reflected") || reflected || reflected_property ||
      reflected_accessor || reflected_method || reflected_overload || reflected_nested || owned ||
      object || object_facade || factory || factory_owned || constructor || constructor_owned ||
      codec || codec_view || extensions || reflected_object || object_factory || pointer ||
      handle || enum_value || schema || schema.raw() != nullptr || proxy_capability || proxy_view ||
      extensions.find<caors_plugin_protocol_meta_extension>(reflected_id).has_value() || sequence ||
      set || associative || pair || tuple || variant || indirect) {
    control->release(control->context);
    (void)invocation.get();
    return fail("detach 后 registry 句柄失效或已绑定容器 view 保活异常");
  }
  control->release(control->context);
  if (!invocation.get()) {
    return fail("detach 前已进入的 method 没有安全完成");
  }
  invocation = {};
  object = {};
  object_facade = {};
  sequence = {};
  set = {};
  associative = {};
  pair = {};
  tuple = {};
  variant = {};
  indirect = {};
  pointer = {};
  handle = {};
  enum_value = {};
  schema = {};
  proxy_capability = {};
  proxy_view = {};
  extensions = {};
  factory = {};
  constructor = {};
  codec_view = {};
  codec = {};
  constructor_owned.reset();
  factory_owned.reset();
  owned.reset();
  if (control->destructions(control->context) != 3) {
    return fail("detach 后 owning value 没有在 DLL 保活期间安全析构");
  }
  reflected_property = {};
  reflected_accessor = {};
  reflected_method = {};
  reflected_overload = {};
  reflected_nested = {};
  reflected_object = {};
  object_factory = {};
  reflected = {};
  if (manager.unload(managed_id)) {
    return fail("释放旧反射视图后 plugin manager 仍无法完成卸载");
  }
  if (managed || managed.state() != caors::plugins::plugin_state::unloaded || manager.size() != 0 ||
      manager.find(managed_id)) {
    return fail("plugin manager 卸载后旧句柄没有失效");
  }

  auto reloaded_result = manager.load(argv[1]);
  if (!reloaded_result || reloaded_result->generation() == first_generation ||
      manager.unload(reloaded_result->id())) {
    return fail("plugin manager 重新加载没有生成新的 generation");
  }

  auto failed_managed_result = manager.load(argv[4]);
  if (!failed_managed_result) {
    return fail("plugin manager 无法加载 initialize 失败插件");
  }
  const caors::plugins::plugin failed_managed = *failed_managed_result;
  if (!manager.initialize(failed_managed.id()) ||
      failed_managed.state() != caors::plugins::plugin_state::failed ||
      registry.find_type("caors.test.plugin.failed_draft") || manager.unload(failed_managed.id())) {
    return fail("initialize 失败的 draft 污染了主 registry 或无法清理");
  }

  auto concurrent_result = manager.load(argv[1]);
  if (!concurrent_result || manager.initialize(concurrent_result->id())) {
    return fail("无法为并发查询测试重新加载插件");
  }
  const caors::guid concurrent_id = concurrent_result->id();
  std::atomic<bool> stop_queries{false};
  std::atomic<std::size_t> valid_queries{0};
  std::atomic<std::size_t> absent_queries{0};
  auto querying = std::async(std::launch::async, [&] {
    while (!stop_queries.load(std::memory_order_acquire)) {
      if (registry.find_type("caors.test.plugin.reflected")) {
        valid_queries.fetch_add(1, std::memory_order_relaxed);
      } else {
        absent_queries.fetch_add(1, std::memory_order_relaxed);
      }
    }
  });
  const auto query_deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
  while (valid_queries.load(std::memory_order_acquire) == 0 &&
         std::chrono::steady_clock::now() < query_deadline) {
    std::this_thread::yield();
  }
  const caors::error concurrent_unload = manager.unload(concurrent_id);
  while (absent_queries.load(std::memory_order_acquire) == 0 &&
         std::chrono::steady_clock::now() < query_deadline) {
    std::this_thread::yield();
  }
  stop_queries.store(true, std::memory_order_release);
  querying.get();
  if (valid_queries.load(std::memory_order_acquire) == 0 ||
      absent_queries.load(std::memory_order_acquire) == 0 ||
      (concurrent_unload &&
       concurrent_unload != caors::error{caors::error_domain::plugins, caors::errc::pending}) ||
      (concurrent_unload && manager.unload(concurrent_id))) {
    return fail("并发查询没有一致观察 publish/detach snapshot");
  }

  return 0;
}
