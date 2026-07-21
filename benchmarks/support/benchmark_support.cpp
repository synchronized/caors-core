// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include "caors/registration.h"

#include <charconv>
#include <cstdlib>
#include <iostream>
#include <system_error>

namespace caors::benchmarks {

std::size_t parse_iterations(int argc, char** argv) {
  constexpr std::size_t default_iterations = 100'000;
  if (argc <= 1) {
    return default_iterations;
  }

  std::string_view argument = argv[1];
  if (argument == "-h" || argument == "--help") {
    std::cout << "用法: caors_benchmarks [iterations]\n";
    std::cout << "默认 iterations = " << default_iterations << '\n';
    std::exit(EXIT_SUCCESS);
  }

  std::size_t iterations = default_iterations;
  const char* begin = argument.data();
  const char* end = begin + argument.size();
  const std::from_chars_result result = std::from_chars(begin, end, iterations);
  if (result.ec != std::errc{} || result.ptr != end || iterations == 0) {
    std::cerr << "无效 iterations 参数: " << argument << '\n';
    std::exit(EXIT_FAILURE);
  }

  return iterations;
}

caors::resources::load_result<benchmark_texture_resource>
benchmark_texture_loader::load(caors::resource_guid guid, caors::memory_resource* resource) {
  auto value = caors::memory::make_unique<benchmark_texture_resource>(resource);
  if (value == nullptr) {
    return {{caors::error_domain::resources, caors::errc::out_of_memory}, {}};
  }

  value->set_guid(guid);
  value->width = 256;
  value->height = 128;
  return {{}, std::move(value)};
}

benchmark_resource_io_fixture::benchmark_resource_io_fixture(caors::memory_resource* resource)
    : files(resource), provider(resource), cache(resource), source(files), loader(provider) {
  const caors::error write_status =
      files.write_all("textures/hero.texture.json", benchmark_texture_json);
  if (write_status.failed()) {
    std::cerr << "写入 benchmark 内存文件失败\n";
    std::exit(EXIT_FAILURE);
  }

  const caors::error bind_status =
      provider.bind(benchmark_texture_guid, source, "textures/hero.texture.json");
  if (bind_status.failed()) {
    std::cerr << "绑定 benchmark provider 失败\n";
    std::exit(EXIT_FAILURE);
  }
}

caors::serialization::type_schema make_benchmark_actor_schema() {
  return caors::serialization::make_type_schema({
      caors::serialization::make_field("id"),
      caors::serialization::make_field("health"),
      caors::serialization::make_field("active"),
      caors::serialization::make_field("name"),
  });
}

caors::variant make_benchmark_actor_variant_direct(const benchmark_actor& actor) {
  caors::variant::object_type members;
  members.reserve(4);
  members.push_back(caors::meta::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(static_cast<std::int64_t>(actor.id)),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"health"},
      0,
      caors::variant::from(static_cast<double>(actor.health)),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"active"},
      0,
      caors::variant::from(actor.active),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"name"},
      0,
      caors::variant::from(std::string_view{actor.name.data(), actor.name.size()}),
  });
  return caors::variant::object(std::move(members));
}

caors::error write_benchmark_actor_json_direct(caors::serialization::writer& writer,
                                               const benchmark_actor& actor,
                                               const caors::serialization::type_schema& schema) {
  caors::error status = writer.begin_object(schema);
  if (status.failed()) {
    return status;
  }

  status = writer.begin_field(schema.fields[0]);
  if (status.failed()) {
    return status;
  }
  status = writer.write_i64(static_cast<std::int64_t>(actor.id));
  if (status.failed()) {
    return status;
  }
  status = writer.end_field();
  if (status.failed()) {
    return status;
  }

  status = writer.begin_field(schema.fields[1]);
  if (status.failed()) {
    return status;
  }
  status = writer.write_f64(static_cast<double>(actor.health));
  if (status.failed()) {
    return status;
  }
  status = writer.end_field();
  if (status.failed()) {
    return status;
  }

  status = writer.begin_field(schema.fields[2]);
  if (status.failed()) {
    return status;
  }
  status = writer.write_bool(actor.active);
  if (status.failed()) {
    return status;
  }
  status = writer.end_field();
  if (status.failed()) {
    return status;
  }

  status = writer.begin_field(schema.fields[3]);
  if (status.failed()) {
    return status;
  }
  status = writer.write_string(std::string_view{actor.name.data(), actor.name.size()});
  if (status.failed()) {
    return status;
  }
  status = writer.end_field();
  if (status.failed()) {
    return status;
  }

  return writer.end_object();
}

void print_memory_statistics(std::string_view name, const caors::memory_statistics& statistics,
                             std::size_t iterations) {
  const double allocations_per_iteration =
      static_cast<double>(statistics.allocation_count) / static_cast<double>(iterations);
  const double bytes_per_iteration =
      static_cast<double>(statistics.bytes_allocated) / static_cast<double>(iterations);

  std::cout << name << ": allocations=" << statistics.allocation_count << " ("
            << allocations_per_iteration << "/iter), bytes=" << statistics.bytes_allocated << " ("
            << bytes_per_iteration << "/iter), peak=" << statistics.peak_bytes_in_use
            << ", live=" << statistics.bytes_in_use << '\n';
}

void ensure_benchmark_types_registered() {
  static bool registered = false;
  if (registered) {
    return;
  }

  const caors::error registration_status =
      caors::registration::type_<benchmark_actor>("caors.benchmark.actor")
          .meta()
          .property("id", &benchmark_actor::id)
          .property("health", &benchmark_actor::health)
          .property("active", &benchmark_actor::active)
          .property("name", &benchmark_actor::name)
          .property("health_int", &benchmark_actor::health_int, &benchmark_actor::set_health_int)
          .method("sum", &benchmark_actor::sum)
          .serialization()
          .field("id")
          .field("health")
          .field("active")
          .field("name")
          .commit();
  if (registration_status.failed()) {
    std::cerr << "注册 benchmark 类型失败\n";
    std::exit(EXIT_FAILURE);
  }

  const caors::error texture_registration_status =
      caors::registration::type_<benchmark_texture_resource>("caors.benchmark.texture_resource")
          .meta()
          .property("width", &benchmark_texture_resource::width)
          .property("height", &benchmark_texture_resource::height)
          .serialization()
          .field("width")
          .field("height")
          .commit();
  if (texture_registration_status.failed()) {
    std::cerr << "注册 benchmark 资源类型失败\n";
    std::exit(EXIT_FAILURE);
  }

  registered = true;
}

} // namespace caors::benchmarks
