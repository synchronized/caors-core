// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io.h"
#include "caors/meta.h"
#include "caors/registration.h"
#include "caors/serialization.h"
#include "caors/version.h"
#include "removed_io_api.h"

#include <iostream>
#include <memory>
#include <optional>

namespace {

struct consumer_record {
  int id = 0;
  int count = 0;
};

[[nodiscard]] bool check(caors::error status, const char* step) {
  if (!status) {
    return true;
  }

  std::cerr << step << " 失败，错误码：" << static_cast<int>(status.value()) << '\n';
  return false;
}

[[nodiscard]] bool check_owned_io() {
  std::weak_ptr<caors::io::memory_backend> weak;
  caors::io::vfs_snapshot snapshot;
  {
    caors::io::vfs routes;
    auto memory = caors::io::make_memory_backend();
    if (memory->store("data", "payload") || routes.mount("caors://assets", memory))
      return false;
    weak = memory;
    auto captured = routes.snapshot();
    if (!captured)
      return false;
    snapshot = std::move(captured.value);
    routes.clear();
  }
  if (weak.expired())
    return false;
  {
    caors::io::io_context io{std::move(snapshot), {caors::tasks::inline_executor()}};
    auto reading = io.read("caors://assets/data", {.max_bytes = 7});
    if (!reading.done() || !*reading.result() || reading.result()->value.size() != 7)
      return false;
    auto opening = io.open("caors://assets/data");
    if (!opening.done() || !*opening.result())
      return false;
    auto handle = opening.result()->value;
    auto range = handle.read({.offset = 1, .length = 3});
    if (!range.done() || !*range.result() || range.result()->value.size() != 3)
      return false;
    auto closed = handle.close();
    if (!closed.done() || !*closed.result())
      return false;
  }
  return weak.expired();
}

[[nodiscard]] bool register_consumer_record() {
  return check(caors::registration::type_<consumer_record>("caors.test.installed.consumer_record")
                   .meta()
                   .property("id", &consumer_record::id)
                   .property("count", &consumer_record::count)
                   .serialization()
                   .commit(),
               "注册 consumer_record");
}

} // namespace

int main() {
  const caors::version linked_version = caors::get_version();
  if (linked_version != caors::k_version ||
      caors::get_version_string() != caors::k_version_string) {
    std::cerr << "安装头文件与链接库版本不一致\n";
    return 1;
  }

  if (!check_owned_io() || !register_consumer_record()) {
    return 1;
  }

  consumer_record source{42, 7};
  const caors::meta::type reflected = caors::meta::find<consumer_record>();
  const caors::meta::property id_property = reflected.find_property("id");
  const std::optional<int> reflected_id = id_property.try_get<int>(caors::meta::view(source));
  if (!reflected.registered() || !id_property || reflected_id != source.id) {
    std::cerr << "安装包 meta 属性查询失败\n";
    return 1;
  }

  const caors::serialization::serialization_text_result json = caors::to_json(source);
  if (json.failed()) {
    std::cerr << "安装包 JSON 写出失败，错误码：" << static_cast<int>(json.error().value()) << '\n';
    return 1;
  }

  consumer_record decoded{};
  if (!check(caors::read_json_from(decoded, *json), "安装包 JSON 读回")) {
    return 1;
  }

  if (decoded.id != source.id || decoded.count != source.count) {
    std::cerr << "安装包 JSON round trip 结果不一致\n";
    return 1;
  }

  std::cout << "CAORS " << caors::get_version_string() << " installed consumer passed\n";
  return 0;
}
