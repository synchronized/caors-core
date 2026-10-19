// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SRC_TASKS_TASK_QUEUE_H_
#define CAORS_SRC_TASKS_TASK_QUEUE_H_

#include "caors/tasks/task.h"

#include <utility>

namespace caors::tasks::detail {

// 调用方负责同步；空队列及摘取整批任务均不分配，关闭路径不依赖额外可用内存。
class task_queue final {
  struct node final {
    explicit node(task value) : next_task(std::move(value)) {}
    task next_task;
    node* next = nullptr;
  };

public:
  explicit task_queue(memory_resource* resource) noexcept : resource_(resource) {}
  task_queue(const task_queue&) = delete;
  task_queue& operator=(const task_queue&) = delete;
  ~task_queue() noexcept { clear(); }

  bool empty() const noexcept { return head_ == nullptr; }
  std::size_t size() const noexcept { return count_; }

  void push_back(task&& next) {
    auto* entry = caors::make<node>(resource_, std::move(next));
    if (tail_ != nullptr) {
      tail_->next = entry;
    } else {
      head_ = entry;
    }
    tail_ = entry;
    ++count_;
  }

  task take_front() noexcept {
    node* entry = head_;
    head_ = entry->next;
    if (head_ == nullptr) {
      tail_ = nullptr;
    }
    --count_;
    task result = std::move(entry->next_task);
    caors::destroy(entry, resource_);
    return result;
  }

  void swap(task_queue& other) noexcept {
    std::swap(head_, other.head_);
    std::swap(tail_, other.tail_);
    std::swap(count_, other.count_);
    std::swap(resource_, other.resource_);
  }

  void clear() noexcept {
    while (!empty()) {
      (void)take_front();
    }
  }

private:
  memory_resource* resource_;
  node* head_ = nullptr;
  node* tail_ = nullptr;
  std::size_t count_ = 0;
};

} // namespace caors::tasks::detail

#endif
