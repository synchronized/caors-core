// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/native_file_system.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <utility>

namespace caors::io {
namespace {

[[nodiscard]] std::filesystem::path to_native_path(path_view path) {
  return std::filesystem::path{std::string{path.string_view()}};
}

[[nodiscard]] std::filesystem::path native_path(path_view root, path_view path) {
  std::filesystem::path result = to_native_path(path);
  if (root.empty() || result.is_absolute()) {
    return result;
  }

  return to_native_path(root) / result;
}

[[nodiscard]] std::filesystem::path native_directory_path(path_view root, path_view directory) {
  if (!directory.empty()) {
    return native_path(root, directory);
  }
  if (!root.empty()) {
    return to_native_path(root);
  }

  return std::filesystem::path{"."};
}

[[nodiscard]] path append_child_path(path_view directory, std::string_view name,
                                     memory_resource* resource) {
  if (directory.empty()) {
    return path{name, resource};
  }

  const std::string_view parent = directory.string_view();
  core::string result{allocator<char>{resource}};
  result.reserve(parent.size() + 1 + name.size());
  result.append(parent.begin(), parent.end());
  if (!parent.empty() && parent.back() != '/' && parent.back() != '\\') {
    result.push_back('/');
  }
  result.append(name.begin(), name.end());
  return path{path_view{result}, resource};
}

[[nodiscard]] bool compare_entries(const directory_entry& lhs,
                                   const directory_entry& rhs) noexcept {
  return lhs.path.string_view() < rhs.path.string_view();
}

[[nodiscard]] error validate_path(path_view path) noexcept {
  return path.empty() ? error{error_domain::io, errc::invalid_argument} : error{};
}

class native_input_stream final : public input_stream {
public:
  explicit native_input_stream(std::ifstream input) : input_(std::move(input)) {}

  [[nodiscard]] size_result read(std::span<std::byte> buffer) override {
    if (buffer.empty() || eof()) {
      return ok(std::size_t{0});
    }

    input_.read(reinterpret_cast<char*>(buffer.data()),
                static_cast<std::streamsize>(buffer.size()));
    const std::streamsize count = input_.gcount();
    if (count < std::streamsize{0}) {
      return fail<std::size_t>({error_domain::io, errc::backend_failure});
    }
    if (input_.bad()) {
      return fail<std::size_t>({error_domain::io, errc::backend_failure});
    }

    return ok(static_cast<std::size_t>(count));
  }

  [[nodiscard]] bool eof() const noexcept override { return input_.eof(); }

private:
  std::ifstream input_;
};

class native_output_stream final : public output_stream {
public:
  explicit native_output_stream(std::ofstream output) : output_(std::move(output)) {}

  [[nodiscard]] size_result write(std::span<const std::byte> buffer) override {
    if (closed_) {
      return fail<std::size_t>({error_domain::io, errc::invalid_argument});
    }
    if (buffer.empty()) {
      return ok(std::size_t{0});
    }

    output_.write(reinterpret_cast<const char*>(buffer.data()),
                  static_cast<std::streamsize>(buffer.size()));
    if (!output_) {
      return fail<std::size_t>({error_domain::io, errc::backend_failure});
    }

    return ok(buffer.size());
  }

  [[nodiscard]] error flush() override {
    if (closed_) {
      return {};
    }

    output_.flush();
    return output_ ? error{} : error{error_domain::io, errc::backend_failure};
  }

  [[nodiscard]] error close() override {
    if (closed_) {
      return {};
    }

    error status = flush();
    if (status) {
      return status;
    }

    output_.close();
    if (!output_) {
      return {error_domain::io, errc::backend_failure};
    }

    closed_ = true;
    return {};
  }

private:
  std::ofstream output_;
  bool closed_ = false;
};

} // namespace

native_file_system::native_file_system(memory_resource* resource)
    : root_{allocator<char>{resource}} {}

native_file_system::native_file_system(path_view root, memory_resource* resource)
    : native_file_system(resource) {
  set_root(root);
}

memory_resource* native_file_system::resource() const noexcept {
  return root_.get_allocator().resource();
}

path_view native_file_system::root() const noexcept { return path_view{root_}; }

void native_file_system::set_root(path_view root) {
  const std::string_view text = root.string_view();
  root_.assign(text.begin(), text.end());
}

bool native_file_system::exists(path_view path) const {
  if (path.empty()) {
    return false;
  }

  std::error_code ec;
  return std::filesystem::exists(native_path(root(), path), ec) && !ec;
}

directory_entry_list_result native_file_system::list(path_view directory,
                                                     memory_resource* resource) {
  const std::filesystem::path native_directory = native_directory_path(root(), directory);

  std::error_code ec;
  const bool exists = std::filesystem::exists(native_directory, ec);
  if (ec) {
    return fail<directory_entry_list>({error_domain::io, errc::backend_failure});
  }
  if (!exists) {
    return fail<directory_entry_list>({error_domain::io, errc::not_found});
  }
  if (!std::filesystem::is_directory(native_directory, ec)) {
    if (ec) {
      return fail<directory_entry_list>({error_domain::io, errc::backend_failure});
    }
    return fail<directory_entry_list>({error_domain::io, errc::invalid_argument});
  }

  directory_entry_list entries{allocator<directory_entry>{resource}};
  std::filesystem::directory_iterator iterator{native_directory, ec};
  if (ec) {
    return fail<directory_entry_list>({error_domain::io, errc::backend_failure});
  }

  for (const std::filesystem::directory_entry& item : iterator) {
    const std::string name = item.path().filename().string();
    if (name.empty()) {
      continue;
    }

    std::error_code item_ec;
    const file_kind kind = item.is_directory(item_ec) ? file_kind::directory : file_kind::file;
    if (item_ec) {
      return fail<directory_entry_list>({error_domain::io, errc::backend_failure});
    }

    entries.push_back(directory_entry{
        path{std::string_view{name}, resource},
        append_child_path(directory, name, resource),
        kind,
    });
  }

  std::sort(entries.begin(), entries.end(), compare_entries);
  return ok(std::move(entries));
}

byte_buffer_result native_file_system::read_all(path_view path, memory_resource* resource) {
  error status = validate_path(path);
  if (status) {
    return fail<byte_buffer>(status);
  }

  if (!exists(path)) {
    return fail<byte_buffer>({error_domain::io, errc::not_found});
  }

  std::ifstream input{native_path(root(), path), std::ios::binary | std::ios::ate};
  if (!input) {
    return fail<byte_buffer>({error_domain::io, errc::backend_failure});
  }

  const std::streampos end = input.tellg();
  if (end < std::streampos{0}) {
    return fail<byte_buffer>({error_domain::io, errc::backend_failure});
  }

  byte_buffer bytes = make_byte_buffer(resource);
  bytes.resize(static_cast<std::size_t>(end));
  input.seekg(0, std::ios::beg);
  if (!bytes.empty()) {
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input) {
      return fail<byte_buffer>({error_domain::io, errc::backend_failure});
    }
  }

  return ok(std::move(bytes));
}

input_stream_result native_file_system::open_read(path_view path, memory_resource* resource) {
  error status = validate_path(path);
  if (status) {
    return fail<input_stream_ptr>(status);
  }

  if (!exists(path)) {
    return fail<input_stream_ptr>({error_domain::io, errc::not_found});
  }

  std::ifstream input{native_path(root(), path), std::ios::binary};
  if (!input) {
    return fail<input_stream_ptr>({error_domain::io, errc::backend_failure});
  }

  memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
  auto* stream = memory::make<native_input_stream>(resolved, std::move(input));
  input_stream_ptr result{stream};
  result.get_deleter().reset(resolved, [](void* user_data, void* stored) noexcept {
    memory::destroy(static_cast<native_input_stream*>(stored),
                    static_cast<memory_resource*>(user_data));
  });
  return ok(std::move(result));
}

output_stream_result native_file_system::open_write(path_view path, memory_resource* resource) {
  error status = validate_path(path);
  if (status) {
    return fail<output_stream_ptr>(status);
  }

  std::ofstream output{native_path(root(), path), std::ios::binary | std::ios::trunc};
  if (!output) {
    return fail<output_stream_ptr>({error_domain::io, errc::backend_failure});
  }

  memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
  auto* stream = memory::make<native_output_stream>(resolved, std::move(output));
  output_stream_ptr result{stream};
  result.get_deleter().reset(resolved, [](void* user_data, void* stored) noexcept {
    memory::destroy(static_cast<native_output_stream*>(stored),
                    static_cast<memory_resource*>(user_data));
  });
  return ok(std::move(result));
}

error native_file_system::write_all(path_view path, std::span<const std::byte> bytes) {
  error status = validate_path(path);
  if (status) {
    return status;
  }

  std::ofstream output{native_path(root(), path), std::ios::binary | std::ios::trunc};
  if (!output) {
    return {error_domain::io, errc::backend_failure};
  }

  if (!bytes.empty()) {
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output) {
      return {error_domain::io, errc::backend_failure};
    }
  }

  return {};
}

native_file_system& get_native_file_system() noexcept {
  static native_file_system instance;
  return instance;
}

} // namespace caors::io
