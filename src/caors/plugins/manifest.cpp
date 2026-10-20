// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/manifest.h"

#include "caors/core/string_name.h"
#include "caors/serialization/operations.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>

namespace caors::plugins {
namespace {

[[nodiscard]] error manifest_error(errc code = errc::invalid_argument) noexcept {
  return {error_domain::plugins, code};
}

[[nodiscard]] std::string_view view_of(const core::string& value) noexcept {
  return {value.data(), value.size()};
}

[[nodiscard]] const variant* member(const variant& object, std::string_view name) {
  return object.find_member_variant(core::string_name{name});
}

[[nodiscard]] const core::string* string_member(const variant& object, std::string_view name) {
  const variant* value = member(object, name);
  return value != nullptr ? value->try_as_string() : nullptr;
}

[[nodiscard]] bool read_u32(const variant& object, std::string_view name, std::uint32_t& target) {
  const variant* value = member(object, name);
  std::uint64_t parsed = 0;
  if (value == nullptr) {
    return false;
  }
  if (const auto* number = value->try_as_u64()) {
    parsed = *number;
  } else if (const auto* signed_number = value->try_as_i64();
             signed_number != nullptr && *signed_number >= 0) {
    parsed = static_cast<std::uint64_t>(*signed_number);
  } else {
    return false;
  }
  if (parsed > std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }
  target = static_cast<std::uint32_t>(parsed);
  return true;
}

[[nodiscard]] result<plugin_dependency> parse_dependency(const variant& value) {
  if (!value.is_object()) {
    return fail<plugin_dependency>(manifest_error());
  }
  const core::string* id_text = string_member(value, "id");
  if (id_text == nullptr) {
    return fail<plugin_dependency>(manifest_error());
  }
  const auto id = guid::parse(view_of(*id_text));
  if (!id) {
    return fail<plugin_dependency>(manifest_error());
  }

  version_range range{};
  if (const core::string* range_text = string_member(value, "version")) {
    auto parsed = version_range::parse(view_of(*range_text));
    if (!parsed) {
      return fail<plugin_dependency>(parsed.error());
    }
    range = *parsed;
  }

  bool optional = false;
  if (const variant* optional_value = member(value, "optional")) {
    const bool* parsed = optional_value->try_as_bool();
    if (parsed == nullptr) {
      return fail<plugin_dependency>(manifest_error());
    }
    optional = *parsed;
  }
  return ok(plugin_dependency{*id, range, optional});
}

} // namespace

result<plugin_manifest> parse_plugin_manifest(std::string_view text) {
  auto parsed = serialization::parse_json(text);
  if (!parsed || !parsed->is_object()) {
    return fail<plugin_manifest>(manifest_error());
  }

  const variant& root = *parsed;
  const core::string* id_text = string_member(root, "id");
  const core::string* name = string_member(root, "name");
  const core::string* version_text = string_member(root, "version");
  const core::string* library = string_member(root, "library");
  if (id_text == nullptr || name == nullptr || version_text == nullptr || library == nullptr) {
    return fail<plugin_manifest>(manifest_error());
  }

  const auto id = guid::parse(view_of(*id_text));
  auto version = semantic_version::parse(view_of(*version_text));
  plugin_manifest manifest{};
  if (!id || !version || !read_u32(root, "abi", manifest.abi_version)) {
    return fail<plugin_manifest>(manifest_error());
  }

  manifest.id = *id;
  manifest.name.assign(name->data(), name->size());
  manifest.version = *version;
  manifest.library = std::filesystem::path{std::string{library->data(), library->size()}};
  if (const core::string* display_name = string_member(root, "display_name")) {
    manifest.display_name.assign(display_name->data(), display_name->size());
  }

  if (const variant* dependencies = member(root, "dependencies")) {
    const auto* values = dependencies->try_as_array();
    if (values == nullptr) {
      return fail<plugin_manifest>(manifest_error());
    }
    manifest.dependencies.reserve(values->size());
    for (const variant& value : *values) {
      auto dependency = parse_dependency(value);
      if (!dependency) {
        return fail<plugin_manifest>(dependency.error());
      }
      manifest.dependencies.push_back(*dependency);
    }
  }

  if (const variant* capabilities = member(root, "capabilities")) {
    const auto* values = capabilities->try_as_array();
    if (values == nullptr) {
      return fail<plugin_manifest>(manifest_error());
    }
    manifest.capabilities.reserve(values->size());
    for (const variant& value : *values) {
      const core::string* capability = value.try_as_string();
      if (capability == nullptr) {
        return fail<plugin_manifest>(manifest_error());
      }
      manifest.capabilities.emplace_back(capability->data(), capability->size());
    }
  }

  if (!manifest.valid()) {
    return fail<plugin_manifest>(manifest_error());
  }
  return ok(std::move(manifest));
}

result<plugin_manifest> load_plugin_manifest(const std::filesystem::path& path) {
  std::ifstream stream{path, std::ios::binary};
  if (!stream) {
    return fail<plugin_manifest>(manifest_error(errc::not_found));
  }
  const std::string text{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
  if (!stream.good() && !stream.eof()) {
    return fail<plugin_manifest>(manifest_error(errc::backend_failure));
  }
  return parse_plugin_manifest(text);
}

result<plugin_dependency_order>
resolve_plugin_dependencies(std::span<const plugin_manifest> manifests) {
  const std::size_t count = manifests.size();
  std::vector<std::vector<std::size_t>> dependents(count);
  std::vector<std::size_t> indegrees(count, 0);

  for (std::size_t index = 0; index < count; ++index) {
    for (std::size_t other = 0; other < index; ++other) {
      if (manifests[index].id == manifests[other].id) {
        return fail<plugin_dependency_order>(manifest_error(errc::duplicate_id));
      }
    }
    for (const plugin_dependency& dependency : manifests[index].dependencies) {
      const auto found =
          std::find_if(manifests.begin(), manifests.end(), [&](const plugin_manifest& candidate) {
            return candidate.id == dependency.id;
          });
      if (found == manifests.end()) {
        if (dependency.optional) {
          continue;
        }
        return fail<plugin_dependency_order>(manifest_error(errc::not_found));
      }
      if (!dependency.version.contains(found->version)) {
        return fail<plugin_dependency_order>(manifest_error(errc::inconsistent_type));
      }
      const std::size_t dependency_index =
          static_cast<std::size_t>(std::distance(manifests.begin(), found));
      dependents[dependency_index].push_back(index);
      ++indegrees[index];
    }
  }

  plugin_dependency_order order{};
  order.load.reserve(count);
  for (std::size_t emitted = 0; emitted < count; ++emitted) {
    const auto next = std::find(indegrees.begin(), indegrees.end(), 0);
    if (next == indegrees.end()) {
      return fail<plugin_dependency_order>(manifest_error(errc::cycle_detected));
    }
    const std::size_t index = static_cast<std::size_t>(std::distance(indegrees.begin(), next));
    *next = std::numeric_limits<std::size_t>::max();
    order.load.push_back(index);
    for (std::size_t dependent : dependents[index]) {
      --indegrees[dependent];
    }
  }
  order.unload.assign(order.load.rbegin(), order.load.rend());
  return ok(std::move(order));
}

} // namespace caors::plugins
