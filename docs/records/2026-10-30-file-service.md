<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 统一文件服务实施记录

## 范围与环境

2026-10-30，feature/resource-source-boundary，PR #58，本记录与实现同批提交。
Windows Clang Debug 共享库、Emscripten 静态构建，Chrome 155.0.8059.85。
对应[VFS 收敛计划](../plans/vfs-api-consolidation.md)，当前接口及迁移由[IO 参考](../reference/io.md#统一服务迁移)维护。

file_service 直接拥有挂载状态和调度配置，file_batch 提供固定路由访问并继承服务配置。
普通访问、批次与资源捕获共用原后端及不可变路由表，不再公开 vfs/io_context/vfs_snapshot 和两个旧公共头。
服务与批次共用访问实现，禁止通过服务值复制形成未固定的批次；服务不可复制/移动。
范围读取复用原打开、读取、关闭状态机，参数为 offset、length、max_bytes，成功和失败均清理已打开文件。
实现源文件归为 file_service.cpp 和 file_routes.cpp。同步 file_system、流和实际后端实现保留。

## 本机验证

- build/vfs-final-native.ps1：16/16 资源、配置和序列化相关回归，IO 1/1，相关示例通过。
- build/io-removal-special.ps1：真实插件资源卸载 1/1，普通分块示例通过。
- build/io-removal-installed.ps1：原前缀与迁移安装各 6/6；消费者直接使用服务/批次，拒绝旧 context.h/vfs.h 残留。
- build/file-service-headers.ps1：8 个相关公共头独立编译通过。
- Emscripten caors_web_protocol_test/caors_web_browser_smoke 构建，Node 协议 1/1。
- tests/web/browser_test.cjs：真实 Chrome 通过；跨 tick 范围读取在服务离开作用域后仍完成。
- 文档本地链接检查、clang-format 与 git diff --check 通过。

范围专项覆盖成功、EOF 零长度、越界、整数溢出位置、预算不足、关闭失败与实际关闭次数。
批次专项覆盖继承完成调度、服务销毁、重新挂载后仍读取旧来源。
原路由并发更新、精确后端身份、取消、队列拒绝/丢弃、写入与插件寿命测试继续保留。
迁移中发现旧装配的变量声明顺序及示例别名需要调整；构建文件随实现文件改名重新生成后验证通过。

日志为本机 build/file-service-{native,special,installed,headers,web,browser}.log，不作为公开制品。

## 剩余验证与范围

平台 CI 待最终提交后运行，不能沿用此前批次的绿色结果。
本批不增加手动 dispatch/shutdown 或全局在途预算，也不改变文件内容可变性。
旧配置和同步工具路由仍需后续审计，不宣称整体 IO 兼容迁移完成；不以删除 file_system 为验收条件。
PR 保持草稿，不合并或发布，不修改现有标签。
