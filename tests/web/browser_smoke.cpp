// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/tasks.h"
#include <emscripten.h>
namespace {
caors::tasks::execution_context context;
bool completed = false;
void tick(void*) {
  (void)context.drain();
  if (completed) {
    EM_ASM({ document.body.dataset.caorsStatus = "passed"; });
  } else {
    EM_ASM({ document.body.dataset.caorsStatus = "failed"; });
  }
}
}
int main() {
  auto status = context.get_executor().post(caors::tasks::task{[] { completed = true; }});
  if (status || completed) {
    EM_ASM({ document.body.dataset.caorsStatus = "failed"; });
    return 1;
  }
  // 等下一轮宿主事件循环再运行队列，禁止用阻塞轮询代替异步调度。
  emscripten_async_call(&tick, nullptr, 10);
}
