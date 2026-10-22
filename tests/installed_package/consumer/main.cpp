// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/legacy.h"
#include "caors/meta.h"
#include "caors/registration.h"
#include "caors/serialization.h"
#include "caors/version.h"

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
  auto files = std::make_shared<caors::io::memory_file_system>();
  if (!check(files->write_all("data", std::string_view{"payload"}), "准备 IO 数据")) {
    return false;
  }
  std::weak_ptr<caors::io::memory_file_system> weak = files;
  auto mounts = std::make_shared<caors::io::mount_file_system>();
  if (!check(mounts->mount("assets", std::move(files)), "拥有式路径挂载")) {
    return false;
  }
  caors::io::file_system_resolver resolver;
  if (!check(resolver.mount("caors://assets", std::move(mounts), "assets"), "拥有式 URI 挂载")) {
    return false;
  }
  const auto bytes = resolver.read_all("caors://assets/data");
  if (!bytes || bytes->size() != 7 || weak.expired()) {
    std::cerr << "安装包拥有式挂载读取失败\n";
    return false;
  }
  {
    auto snapshot = resolver.snapshot();
    if (!snapshot)
      return false;
    auto resolved = snapshot->resolve("caors://assets/data");
    if (!resolved)
      return false;
    auto opened =
        caors::io::request_open(std::move(resolved.value), caors::tasks::inline_executor());
    if (!opened || !opened->result() || !*opened->result())
      return false;
    auto file = opened->result()->value;
    auto all = caors::io::read_all_chunked(file, {7, 2}, caors::tasks::inline_executor());
    if (!all || !all->result() || !*all->result() || all->result()->value.size() != 7)
      return false;
    std::size_t consumed = 0;
    auto stream = caors::io::consume_chunks(
        file, {7, 3}, caors::tasks::inline_executor(), [&](auto offset, auto block) {
          if (offset != consumed)
            return caors::io::io_error{caors::io::io_errc::backend_failure};
          consumed += block.size();
          return caors::io::io_error{};
        });
    if (!stream || !stream->result() || !*stream->result() || consumed != 7)
      return false;
    auto closed = caors::io::request_close(file, caors::tasks::inline_executor());
    if (!closed || !closed->result() || !*closed->result())
      return false;
  }
  resolver.clear();
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
