<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# MessagePack 序列化后端开发计划

## 文档状态

- 当前状态：阶段 0 至阶段 3 已完成；阶段 4 仅剩 Linux 跨平台验证；
- 目标：在不引入第三方库的前提下，为现有 serialization reader/writer 增加标准 MessagePack 后端；
- 非目标：不设计 CAORS 私有二进制协议，不在首批实现 MessagePack Extension 类型或对象图格式。

## 设计结论

### 格式边界

- 严格使用 MessagePack 标准标记，不增加私有文件头；
- 整数按能够容纳实际值的最短标准编码写出，浮点数首批统一写为 float64；
- 字符串使用 fixstr、str8、str16 或 str32，输入必须是有效长度范围内的原始 UTF-8 字节；
- 数组使用 fixarray、array16 或 array32；
- 对象和字典使用 fixmap、map16 或 map32；
- 对象字段 key 使用 schema 的序列化名称，不使用进程内 `string_name` ID；
- 字典继续允许任意 MessagePack 标量或容器 key；
- 首批拒绝 bin、ext、保留标记以及尾随第二个根值，避免把未建模数据静默转换为字符串。

### 与现有抽象的适配

MessagePack map 必须在容器头中提前写入成员数量，而现有 `writer::begin_object()` 不知道字段过滤后的
实际数量。因此后端不能在 `begin_object()` 时直接写出 map header。

`messagepack_options` 由 MessagePack parser/writer 或对应便捷 API 显式持有，不进入通用
`serialization::context`。同理，JSON 格式选项已经从 context 移到 `json_reader` / `json_writer`
边界；context 只保存跨格式的 schema、resolver、构造、借用存储和 typed traversal 策略。

首批采用容器栈暂存：

1. 标量直接编码为一段完整字节；
2. 数组、对象和字典在栈帧中收集已经完成的子值；
3. `end_array()`、`end_object()` 或 `end_dictionary()` 时根据实际数量生成容器头并拼接子值；
4. 根值完成后提交到拥有式 `io::byte_buffer` 或调用方提供的 `io::output_stream`。

读入端先把一个完整 MessagePack 根值解析为 `meta::variant`，随后复用 `variant_reader`。这样字段旧名、
未知字段策略、schema migration、多态 envelope 和现有递归调度保持同一语义，不复制一套对象导航逻辑。

typed 读入沿用现有 DOM/JSON 的事务边界：格式解析完成前不修改目标；进入字段写入后按顺序提交，后续字段
失败不会回滚此前已经写入的字段。非复制类型、引用成员和多态对象的整对象 clone/commit 不属于
MessagePack 后端职责，需要单独设计通用事务读取能力。

### 资源与限制

- parser 接受 `std::span<const std::byte>` 和 `io::input_stream`；
- DOM、返回 byte buffer 和目标对象使用调用方传入的结果资源；
- parser/writer 暂存结构使用独立的作用域临时资源，具体设计见
  [scoped-temporary-memory.md](scoped-temporary-memory.md)；
- 解析深度、字符串长度、数组长度和 map 长度在分配前校验；
- 首批复用 `context.config.limits.max_depth` 作为 typed read 的递归限制，并为独立 parser 提供格式层限制；
- 所有多字节数值按 MessagePack 规定的大端字节序读写，不能依赖主机字节序；
- 截断输入、非法标记、长度溢出、超过限制和尾随根值必须返回明确错误，不产生部分 DOM。

## 公开 API 草案

```cpp
struct messagepack_options {
  std::uint32_t max_parse_depth = 64;
  std::size_t max_string_size = 64 * 1024 * 1024;
  std::size_t max_container_size = 16 * 1024 * 1024;
};

class messagepack_writer final : public writer {
public:
  messagepack_writer();
  explicit messagepack_writer(io::byte_buffer& output);
  explicit messagepack_writer(io::output_stream& output);

  [[nodiscard]] const io::byte_buffer& bytes() const noexcept;
  void reset();
};

[[nodiscard]] io::byte_buffer_result
encode_messagepack(const variant& value,
                   memory_resource* resource = nullptr);

[[nodiscard]] variant_result
parse_messagepack(std::span<const std::byte> bytes,
                  messagepack_options options = {},
                  memory_resource* resource = nullptr);

[[nodiscard]] variant_result
parse_messagepack(io::input_stream& stream,
                  messagepack_options options = {},
                  memory_resource* resource = nullptr);
```

typed convenience API 在基础 writer/parser 稳定后增加：

```cpp
template <typename T>
[[nodiscard]] result<io::byte_buffer> to_messagepack(const T& object);

template <typename T>
[[nodiscard]] error read_messagepack_from(T& object, std::span<const std::byte> bytes);
```

## 分阶段实施

### 阶段 0：接口审查与计划冻结

- [x] 确认现有 writer 无法提前知道对象实际字段数量；
- [x] 确认 reader 的 schema、未知字段和迁移语义应复用 `variant_reader`；
- [x] 决定首批不引入第三方依赖；
- [x] 决定不扩展 `variant` 的 binary kind，bin/ext 暂时明确拒绝。

### 阶段 1：基础 DOM 编解码

- [x] 增加 `messagepack_options`；
- [x] 实现 `variant` 到 MessagePack byte buffer 的基础 DOM 编码；
- [x] 实现 `messagepack_writer` 的 null、bool、整数、float64、字符串、数组、对象和字典编码；
- [x] 实现 byte span parser，解析为 `meta::variant`；
- [x] 覆盖最短整数编码、大端字节序、截断输入、非法标记、解析限制和尾随数据测试；
- [x] 补齐字符串与容器各级长度标记边界测试。

完成标准：MessagePack 官方兼容编码可以与 `variant` 双向转换，错误输入不产生成功结果。

### 阶段 2：typed serialization 接入

- [x] 增加 `to_messagepack()` 与 `read_messagepack_from()`；
- [x] typed API 复用现有 `serialize()`、`variant_reader` 和 context；
- [x] 覆盖 schema migration 和未知字段策略；
- [x] 覆盖嵌套对象、容器、tuple/variant、多态 envelope 和 external-ref；
- [x] 确认失败时目标值遵守现有事务边界。

完成标准：现有 JSON/DOM 语义测试能够以 MessagePack 后端复用或建立等价用例。

### 阶段 3：流与资源控制

- [x] 接入 `io::input_stream` 和 `io::output_stream`；
- [x] 校验短读、短写和流错误传播；
- [x] 将 parser DOM 和返回 byte buffer 接入结果资源，并将 parser/writer 暂存结构接入作用域临时资源；
- [x] 增加深度、字符串长度和容器长度限制测试。

完成标准：流式入口行为明确，所有外部长度均在分配前受限。

### 阶段 4：文档与跨平台验证

- [x] 更新正式序列化文档和公开头文件自包含测试；
- [x] Windows Clang Shared Debug/Release 全量构建与测试；
- [x] Windows MSVC Debug/Release 全量构建与测试；
- [ ] Linux 环境可用后补充 Clang/GCC 与大小端编码测试向量验证。

Windows Clang Shared Debug/Release 各 83 项测试全部通过；Windows MSVC 静态 Debug/Release
使用单线程完成干净构建，各 81 项测试全部通过。静态构建按设计跳过两组仅适用于共享库的插件测试。

## 暂缓内容

- MessagePack bin 与 extension 类型；
- timestamp extension；
- 流式增量 parser 和不构造 DOM 的 typed reader；
- 零拷贝字符串视图；
- CAORS 私有文件头、校验和、压缩、索引或随机访问格式；
- 对象图身份协议。

这些能力需要单独的使用场景和兼容性约束，不随基础后端默认扩张。
