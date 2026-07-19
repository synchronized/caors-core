// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_WALK_H_
#define CAORS_IO_WALK_H_

#include "caors/io/file_system.h"

#include <cstddef>
#include <functional>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::io {

/** @brief 递归遍历回调返回的控制动作。 */
enum class walk_action {
  /** @brief 继续遍历；目录项会继续进入子目录。 */
  next,
  /** @brief 跳过当前目录项的子目录。 */
  skip_children,
  /** @brief 停止整个遍历，`walk()` 返回成功。 */
  stop,
};

/** @brief 递归遍历选项。 */
struct walk_options {
  /** @brief 内部临时目录列表和路径副本使用的内存资源。 */
  memory_resource* resource = nullptr;
  /** @brief 最大访问深度。默认无限制；不包含 root 时，直接子项深度为 0。 */
  std::size_t max_depth = (std::numeric_limits<std::size_t>::max)();
  /** @brief 是否把 root 自身作为第一条记录传给 visitor。 */
  bool include_root = false;
};

namespace detail {

template <typename> inline constexpr bool always_false_v = false;

struct walk_state {
  bool stopped = false;
};

[[nodiscard]] inline std::string_view last_path_segment(path_view value) noexcept {
  std::string_view text = value.string_view();
  while (!text.empty() && (text.back() == '/' || text.back() == '\\')) {
    text.remove_suffix(1);
  }
  const std::size_t separator = text.find_last_of("/\\");
  return separator == std::string_view::npos ? text : text.substr(separator + 1);
}

template <typename Result>
[[nodiscard]] error normalize_walk_result(Result&& result, walk_action& action) {
  using result_type = std::remove_cvref_t<Result>;

  if constexpr (std::is_same_v<result_type, walk_action>) {
    action = result;
    return {};
  } else if constexpr (std::is_same_v<result_type, error>) {
    action = walk_action::next;
    return result;
  } else if constexpr (std::is_same_v<result_type, bool>) {
    action = result ? walk_action::next : walk_action::stop;
    return {};
  } else {
    static_assert(always_false_v<Result>, "无效的 walk visitor 返回类型");
  }
}

template <typename Visitor>
[[nodiscard]] error invoke_walk_visitor(Visitor& visitor, const directory_entry& entry,
                                        std::size_t depth, walk_action& action) {
  if constexpr (std::is_invocable_v<Visitor&, const directory_entry&, std::size_t>) {
    if constexpr (std::is_void_v<
                      std::invoke_result_t<Visitor&, const directory_entry&, std::size_t>>) {
      std::invoke(visitor, entry, depth);
      action = walk_action::next;
      return {};
    } else {
      return normalize_walk_result(std::invoke(visitor, entry, depth), action);
    }
  } else if constexpr (std::is_invocable_v<Visitor&, const directory_entry&>) {
    if constexpr (std::is_void_v<std::invoke_result_t<Visitor&, const directory_entry&>>) {
      std::invoke(visitor, entry);
      action = walk_action::next;
      return {};
    } else {
      return normalize_walk_result(std::invoke(visitor, entry), action);
    }
  } else {
    static_assert(always_false_v<Visitor>, "无效的 walk visitor 签名");
  }
}

template <typename Visitor>
[[nodiscard]] error walk_directory(file_system& file_system, path_view directory, std::size_t depth,
                                   Visitor& visitor, const walk_options& options,
                                   walk_state& state) {
  directory_entry_list_result entries = file_system.list(directory, options.resource);
  if (entries.failed()) {
    return entries.error();
  }

  for (const directory_entry& entry : *entries) {
    walk_action action = walk_action::next;
    error status = invoke_walk_visitor(visitor, entry, depth, action);
    if (status) {
      return status;
    }
    if (action == walk_action::stop) {
      state.stopped = true;
      return {};
    }
    if (entry.kind == file_kind::directory && action != walk_action::skip_children &&
        depth < options.max_depth) {
      status = walk_directory(file_system, entry.path, depth + 1, visitor, options, state);
      if (status || state.stopped) {
        return status;
      }
    }
  }

  return {};
}

} // namespace detail

/**
 * @brief 基于 `file_system::list()` 递归遍历目录树。
 *
 * visitor 可以接收 `(const directory_entry&, std::size_t depth)` 或 `(const directory_entry&)`。
 * 返回 `void` 表示继续，返回 `bool` 时 `false` 表示停止，返回 `walk_action`
 * 可控制跳过子目录或停止， 返回 `caors::error` 可把错误传出。
 */
template <typename Visitor>
[[nodiscard]] error walk(file_system& file_system, path_view root, Visitor&& visitor,
                         walk_options options = {}) {
  detail::walk_state state{};
  auto&& current_visitor = visitor;

  if (options.include_root) {
    file_status_result root_status = file_system.status(root);
    if (root_status.failed()) {
      return root_status.error();
    }

    directory_entry root_entry{
        path{detail::last_path_segment(root), options.resource},
        path{root, options.resource},
        root_status->kind,
    };
    walk_action action = walk_action::next;
    error status = detail::invoke_walk_visitor(current_visitor, root_entry, std::size_t{0}, action);
    if (status || action == walk_action::stop || action == walk_action::skip_children ||
        root_status->kind != file_kind::directory || options.max_depth == 0) {
      return status;
    }

    return detail::walk_directory(file_system, root, std::size_t{1}, current_visitor, options,
                                  state);
  }

  return detail::walk_directory(file_system, root, std::size_t{0}, current_visitor, options, state);
}

} // namespace caors::io

#endif
