# 子任务 T08：独立响应回调生命周期修复
最后修改时间: 2026-11-01 19:26:21

## 审查状态

Accepted。总计划修订 2，从原 T02 抽取已确定的独立工作，不增加功能范围。用户的整体实现授权适用。

- [x] MCP-T08-001 已于 2026-10-31 22:53:54 accepted；9 项回调回归继续包含在最终 166 项定向检查中。
- [ ] 最终真实 Process 切换、断线和退出联动仍需 T07 证据，见[验证清单](../../verification/20261031-ai-character-mcp.md)。

下文是本任务局部交付记录，本次仅同步当前状态；不改历史产物/证据、不重开任务。

## 父任务与权威记录

- [意图](../../intent/20261031-ai-character-mcp.md)、[权威总计划](../../plan/20261031-ai-character-mcp.md)。
- 任务 T08，分配 MCP-T08-001，执行者主会话 `01a19088-7df4-77a3-bbd4-1a8f512a7563`；工作区 `D:\SourceCodes\opensource\mir2x`。

## 目标与验收

为 A04、A05、A08 修复单调响应 ID、超时与 Process 切换取消、迟到回执处理。无需依赖新桥接传输；作为 T02 的额外前置。

## 范围与文件

- 新增 `client/src/responsecallbacks.hpp`、`client/test/unit/test_response_callbacks.cpp`。
- 修改 `client/src/client.cpp/.hpp` 的响应管理，不接入 AI 控制器或修改 GUI。
- 测试 target 为 `test_response_callbacks`，由 T01 负责人统一写入共享测试 CMake，避免并发编辑。

## 输入

无前置。基线 `b1ddda8d8742afb1d60839f759733a2e8ee26207`，已有共享文档和已登记 T01 产品修改属于预期；不覆盖其文件。

## 局部技术设计

- 抽取不依赖 SDL/网络的 ResponseCallbacks，时钟由调用者传入。响应 ID 从 1 单调增长，空表/取消不重置，达到 uint64 上限后拒绝新注册。
- dispatch 在调用用户 handler 前移出并删除记录。expire/cancel 先移除待回调记录再通知，支持回调中注册/取消而不持无效迭代器。
- scoped send 接受明确 timeout_ms 和取消通知，Client 使用 SDL 单调 ticks；原 send 接口保留且默认不强加超时，避免改变未经本任务改造的旧 UI 等待语义。
- Process 切换前取消旧作用域，阻止取消通知期间新增旧作用域请求；超时/取消至多一次。调用方后续使用弱生命周期标记保障 UI 捕获安全。
- 未知/迟到响应只记诊断而不抛异常；析构/显式取消清空 pending。发送失败撤销对应新注册，不能留下永不完成的记录。

## 实施安排

1. 实现纯回调容器和到期/取消/重入测试。
2. 将 Client 现有响应分派接到容器，新增 scoped send 并保持旧 send 兼容。
3. 向 T01 负责人提供测试注册片段，运行独立目标/现有 netio 回归，供 T02 接入。

## 检查与风险

- 测试 ID 不复用/耗尽、未知 ID、分派中注册、分派中取消、重复响应、timeout/cancel 互斥、发送失败清理以及取消回调中再次取消。
- 独立 CMake target 不需要游戏资源；Client 编译与 T01 总构建由主会话协调，不能并发写共享 build。
- 回调作用域不替代业务完成回执；取消后已发送网络行为仍可能发生，T02 负责映射 unknown。

## 交接依据

用户已授权按计划实施。SpecFlow skill/协作契约和总计划为协议入口。主会话直接执行，写回本文接收/交付节并维护总登记，无外部交接。

## 接收与进展

- 接收：2026-10-31，主会话，MCP-T08-001，修订 2，基线已核对。T01 独占其他已登记文件，T08 独占以上文件。

## 交付与审查

- 交付：2026-10-31 22:53:54，主会话，MCP-T08-001，修订 2；实现及必要检查完成。
- 实际变更：新增 ResponseCallbacks，ID 不复用/耗尽拒绝，移出后回调、重入安全、超时及作用域取消；Client 保留旧 send，新增 sendScoped，迟到响应不抛错。回执分派时再次核对 deadline，不能利用网络轮询先于超时轮询误接受迟到回执。
- 开发检查：UCRT64 GCC 使用 -std=c++26 -O2 -Wall -Wextra -Werror，链接仓库现有 Catch2，独立执行 9 场景/54 断言全部通过，产物 `.runtime/test-response-callbacks/test_response_callbacks.exe`。
- 正式入口：T01 负责人登记 `test_response_callbacks` 并运行 CTest，9/9 通过；证据 `.runtime/collaboration/t08-ctest.log`。
- 客户端接入：从当前 Ninja 提取 client.cpp 实际编译参数，去除输出/依赖文件参数后执行 -fsyntax-only，通过；没有并发写共享 build。
- 未运行项：完整客户端链接、真实游戏生命周期/交易回归留 T02/T07；本任务不宣称游戏端到端完成。
- 审查：主会话已检查实际差异、异常/重入边界和测试结果，接受此独立前置交付。

| 文件 | SHA-256 |
| --- | --- |

| client/src/responsecallbacks.hpp | c8287b9ac85bd6cddc2ed33a7ba3290ff35dcd91c79de001c3301449caffaf5d |
| client/src/client.hpp | 3a7f9ad4b93e7937987232b235be6291959243a7dd7729d637e18fae1362dcd9 |
| client/src/client.cpp | 50623f7c6525a40a83d0f52eb842761789e6ff84d37dee180213e46a5a840001 |
| client/test/unit/test_response_callbacks.cpp | a42956ee695333988ec93c96dcf0cfdd0f6500b62cc21fa0aee6852234644ab9 |
