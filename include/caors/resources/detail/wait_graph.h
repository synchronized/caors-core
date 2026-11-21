// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_RESOURCES_DETAIL_WAIT_GRAPH_H_
#define CAORS_RESOURCES_DETAIL_WAIT_GRAPH_H_

#include "caors/core/containers.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>

namespace caors::resources::detail {

// 记录实际在途操作，不以资源键代替操作身份；不拥有请求、对象或回调。
class wait_graph final {
public:
  using node_id = std::uint64_t;
  enum class link_result { linked, cycle, missing_node, limit };

  explicit wait_graph(memory_resource* resource = nullptr, std::size_t node_limit = 4096,
                      std::size_t edge_limit = 16384)
      : resource_(resource ? resource : get_default_memory_resource()),
        nodes_(typename node_map::allocator_type{resource_}), node_limit_(node_limit),
        edge_limit_(edge_limit) {}

  [[nodiscard]] std::optional<node_id> insert() {
    std::lock_guard lock{mutex_};
    if (nodes_.size() >= node_limit_ || next_ == std::numeric_limits<node_id>::max())
      return {};
    const auto id = next_;
    nodes_.emplace(id, edge_list{allocator<edge>{resource_}});
    ++next_;
    return id;
  }

  // 检查与插边共用锁，两个根并发提出相反依赖时最多接受一条边。
  [[nodiscard]] link_result link(node_id parent, node_id child) {
    std::lock_guard lock{mutex_};
    const auto from = nodes_.find(parent);
    if (from == nodes_.end() || !nodes_.contains(child))
      return link_result::missing_node;
    if (parent == child)
      return link_result::cycle;
    auto& edges = from->second;
    auto existing = std::find_if(edges.begin(), edges.end(),
                                 [child](const edge& value) { return value.target == child; });
    if (existing != edges.end()) {
      if (existing->subscriptions == std::numeric_limits<std::size_t>::max())
        return link_result::limit;
      ++existing->subscriptions;
      return link_result::linked;
    }
    if (reachable(child, parent))
      return link_result::cycle;
    if (edges_ >= edge_limit_)
      return link_result::limit;
    edges.push_back({child, 1});
    ++edges_;
    return link_result::linked;
  }

  // 同一父子操作可有多个依赖订阅，只释放本次订阅的等待关系。
  void unlink(node_id parent, node_id child) noexcept {
    std::lock_guard lock{mutex_};
    auto from = nodes_.find(parent);
    if (from == nodes_.end())
      return;
    auto& edges = from->second;
    auto found = std::find_if(edges.begin(), edges.end(),
                              [child](const edge& value) { return value.target == child; });
    if (found != edges.end() && --found->subscriptions == 0) {
      edges.erase(found);
      --edges_;
    }
  }

  // 操作终结时清理所有入边和出边；迟到的订阅释放可安全重复执行。
  void erase(node_id id) noexcept {
    std::lock_guard lock{mutex_};
    const auto found = nodes_.find(id);
    if (found == nodes_.end())
      return;
    edges_ -= found->second.size();
    nodes_.erase(found);
    for (auto& [key, edges] : nodes_) {
      const auto removed =
          std::erase_if(edges, [id](const edge& value) { return value.target == id; });
      edges_ -= removed;
    }
  }

private:
  struct edge {
    node_id target;
    std::size_t subscriptions;
  };
  using edge_list = core::vector<edge>;
  using node_map = core::hash_map<node_id, edge_list>;

  bool reachable(node_id start, node_id target) const {
    core::vector<node_id> pending{allocator<node_id>{resource_}};
    core::hash_set<node_id> visited{typename core::hash_set<node_id>::allocator_type{resource_}};
    pending.push_back(start);
    while (!pending.empty()) {
      const auto current = pending.back();
      pending.pop_back();
      if (current == target)
        return true;
      if (!visited.insert(current).second)
        continue;
      for (const auto& value : nodes_.at(current))
        pending.push_back(value.target);
    }
    return false;
  }

  memory_resource* resource_;
  node_map nodes_;
  std::size_t node_limit_;
  std::size_t edge_limit_;
  std::size_t edges_ = 0;
  node_id next_ = 1;
  std::mutex mutex_;
};
} // namespace caors::resources::detail
#endif
