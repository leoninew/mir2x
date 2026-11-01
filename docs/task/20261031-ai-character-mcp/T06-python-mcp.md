# 子任务 T06：Python MCP 工具、资源与宿主接入
最后修改时间: 2026-11-01 19:26:21

## 审查状态

Accepted。用户已接受总计划修订 3 并授权 Implementation；本任务执行分配与依赖以总计划为准。

- [x] MCP-T06-001 局部实现已于 2026-11-01 08:09:56 accepted，后补示例文档纳入 23 文件交付；76/76、ruff 和官方 SDK/fake 桥接检查通过。
- [ ] 最终实际 C++ 35 能力、宿主及真实角色全链路尚待验证，见[验证清单](../../verification/20261031-ai-character-mcp.md)。

下文为 T06 交付及后补证据。交付时尚未实现的 C++ 域适配现已由 T02 至 T05 接入，但不因此推断实际角色通过。此次仅同步说明文档，保留 T06 历史 manifest；README 的后续文档哈希变化不代表 Python 运行代码改变。

## 父任务与权威记录

- [总意图](../../intent/20261031-ai-character-mcp.md)、[权威总计划](../../plan/20261031-ai-character-mcp.md)。任务 T06，协调根 `D:\SourceCodes\opensource\mir2x`。

## 目标与验收

将全部首版能力通过 Python 官方 MCP SDK 提供给宿主，返回符合契约的结构化数据；完成真实 stdio 初始化、tools/resources 列举与调用测试。覆盖 A01、A02、A03、A05、A06、A07、A08 的 MCP 接入部分。

## 范围与文件

- 新增 `tools/mir2x-mcp/pyproject.toml`、`uv.lock`、`src/mir2x_mcp/`、`tests/`、`examples/` 及中文 README。
- 模块分为 `__main__/config`、`bridge_client`、`models`、`tools`、`resources`；日志只写 stderr 和受限的运行日志文件。
- 不修改游戏 C++、共享 schema、全局宿主配置或普通游戏账号设置。契约缺口通过主会话更新 T01 及方案修订，不能用 Python 私有字段绕开。

## 输入

直接前置 T01 已接受，包含实际认证传输、全部域方法 schema、生命周期字段和六态操作 fixture。控制器运行行为按确定契约 fake，最终由 T07 与真实 T02 集成。本任务不依赖 T05 已实现，可以用 fake bridge 开发全部工具；T07 再证明与所有真实域适配相容。

## 局部技术设计

1. 使用 Windows Python >=3.13,<3.14（本机 3.13.11）、`mcp==2.3.0` 和 Pydantic v2；uv 锁定解析出的传递版本。开发依赖 pytest、pytest-asyncio、ruff；不安装到全局，不与 MSYS2 构建 Python 共用包目录。
2. 入口为 `python -m mir2x_mcp`，通过 MCPServer 注册具名、有类型、中文描述的工具与资源，再 `run(transport="stdio")`。SDK 负责 JSON-RPC 协商；自有 IPC 不伪装成另一套 MCP HTTP 服务。
3. 参数配置：`--bridge-dir`、可选 `--instance-id`，均由宿主启动配置传入。目录内多个有效实例返回 ambiguous_instance；选定后只连接该实例，不能自动切到新客户端。配置不包含 token，凭据读取后不出现在模型可见结果中。
4. `bridge_client` 使用 asyncio streams，严格读固定长度帧/限制输出；认证、读写超时及出站锁独立实现。按 id 匹配多个并发只读查询，动作在 C++ 串行约束基础上也提前返回 busy；一个已认证连接供全部工具/资源复用。
5. 工具映射总计划完整清单，参数为封闭 typed 模型；动作要求 request_id、context 和有界 timeout_ms。传参前校验，但 C++ 是权限最终执行点。不得把 shell、路径或原始游戏报文作为“高级工具”。
6. 成功返回 MCP structuredContent 与简短中文说明；校验/业务失败标记工具错误并保留稳定 code。unknown 是操作结果，不伪装 Python 异常或失败可重试提示。高风险工具 annotations 标为写入/可能破坏；annotations 仅供宿主理解，不充当授权。
7. 资源与查询工具调用同一数据模型。未登录资源返回明确阶段和不可用原因；不编造空角色。外部游戏文本放在标明来源的数据字段中，不拼进系统提示或修改工具描述。
8. 已认证连接每 2 秒心跳；断线把未收结果的写调用标成未知/transport_lost，禁止自动重发。允许只读重连原实例，重新读取 context 和 paused 状态；旧写请求上下文不可复用。
9. 宿主取消一次写调用时，对该 operation 请求停止/取消并明确结果可能已提交；请求尚无 operation_id 则暂停该连接控制，不直接假设“未发生”。stdio EOF/进程结束关闭桥接，客户端因断开暂停。
10. stdout 只用于 MCP 协议。不得打印 uv 安装日志/调试信息到 stdout。首次 `uv sync --locked` 独立于宿主启动；运行配置直接用项目 `.venv/Scripts/python.exe` 加 `-m mir2x_mcp`，可提供 `uv run --locked --no-sync --project ...` 的等效配置，避免宿主每次启动安装。

## 实施安排

1. 建立 pyproject、包入口、开发检查和锁文件，固定 schema fixture 读取路径。
2. 实现认证/实例选择/有界 IPC/心跳/断线及 typed models。
3. 注册全部工具和六个资源；分别给查询、动作、业务错误与 unknown 编写示例。
4. 编写 fake bridge 覆盖全部工具参数/响应，并用官方 SDK Client 真正以 subprocess stdio 调用服务。
5. 提供中文项目说明、宿主中立的 JSON 配置样例和仅路径占位符，不自动编辑用户全局设置；向 T07 交付可启动命令及检查证据。

## 检查与风险

- 按总计划设置仓库内 UV_CACHE_DIR/UV_PYTHON_INSTALL_DIR，运行 `uv sync --project tools/mir2x-mcp --locked --group dev`。
- `uv run --project tools/mir2x-mcp --locked pytest`，以及 ruff check/format --check；首次生成 lock 在实施时完成，不能以缺 lock 为由跳过锁定验证。
- 使用官方 SDK Client 子进程 transport 测试 initialize、list_tools、list_resources、read_resource、call_tool 和正常关闭，验证 stdout 不含杂音。直接调用 Python 函数不算 MCP 集成验证。
- 对全部工具断言 schema 名称、必填字段、64 位字符串、拒绝 extra 字段、逐项 fixture 往返；检查只读/写入 annotations 和 structuredContent。
- 覆盖无实例、多个实例、错误 token/协议、旧二进制、陈旧凭据、连接中断、响应分片、过大帧、超时、工具取消、宿主 EOF 及原实例退出后出现另一实例。
- 写调用“服务器已收但响应丢失”只返回 unknown、不重试；资源查询可重连但不能取得 active 权限。SDK API 依据官方 v2.3.0，本次不兼容旧版 FastMCP API。

## 交接依据

- 已分配 MCP-T06-001 给主会话。SpecFlow 入口 `C:\Users\leon\.codex\plugins\cache\specflow-local\specflow\0.2.1\skills\specflow\SKILL.md` 和 `references/task-coordination.md`。
- 当前已授权 Implementation；总计划提供后续授权、编号、基线和 T02 输入版本。写回本文接收与交付节，主会话读取；跨会话由用户转交摘要。

## 接收与进展

已接收：2026-11-01 07:49:28，主会话，MCP-T06-001，方案修订3；T01的24文件manifest与24/24测试已核对接受。Python工作目录尚不存在，无待保留产品文件。

| 时间 | 分配编号 / 执行者 | 接收、进展或阻塞 | 证据 / 回报位置 |
| --- | --- | --- | --- |
| 2026-10-31 | 无 / 无 | 仅完成局部计划 | 本文 |

## 交付与审查

MCP-T06-001 已实现并由主会话接受局部交付，以下保留交付时范围和证据。产品范围仅 `tools/mir2x-mcp/`，23个文件的历史交付散列见 `.runtime/collaboration/T06-delivery-manifest.json`；基线 HEAD b1ddda8d8742afb1d60839f759733a2e8ee26207，无暂存或提交。本次 README/任务说明文档更新不改 Python 运行代码，也不覆盖该历史 manifest。

### 实际交付

- 官方 MCPServer stdio，35个静态 typed 工具、6个只读资源，中文描述/宿主配置/项目说明。完整映射见项目 README；当前游戏实际能力仍以客户端 capabilities 为准。
- 同一契约生成固定函数签名、Pydantic模型和包内schema副本；原始参数先严格校验，输出schema与成功响应按共享定义校验。没有generic kwargs/raw packets/文件路径工具。
- 实例与角色绑定、Windows安全凭据读取、长度前缀有界帧、单认证连接、并行只读ID匹配、2秒心跳、写调用不重放、取消关闭桥接、EOF清理。
- `unknown`作为正常操作数据；发送后丢失响应使用transport_lost/side_effects_possible=true/retryable=false，不伪造operation_id。旧写上下文和自动换实例被拒绝。
- SDK默认预解析会改变可选字符串null/[]；生成的字符串注解保留str、仅缺省使用内部None哨兵，原始显式null被契约拒绝。真实stdio测试验证游标/目标句柄字符串原样传递。
- Windows API常量所在模块、pywintypes异常归一化已在实测中修正；开发检查过程发现的问题已修复，最终结果见日志。未修改C++、共享schema或全局宿主配置。

### 输入与依赖版本

- T01接受的v1契约 SHA256：`113ead156c1386af6afe8b129c62c7278c3e366ee2d57556c87d3ac857957668`。包内副本逐字节一致，共享35方法fixture及六态operation fixture全部通过。
- Windows Python 3.13.11、uv 0.11.16；mcp/mcp-types 2.3.0、Pydantic 2.14.2、jsonschema 4.26.0、pywin32 312、pytest 9.2.0、pytest-asyncio 1.4.1、ruff 0.17.3。传递依赖见uv.lock。
- Python独立.venv与仓库内uv缓存；pytest专用临时目录为根.runtime/mcp-python-tests，项目内.runtime/pytest-cache保存测试缓存。

### 开发检查证据

在仓库根目录，UV_CACHE_DIR与UV_PYTHON_INSTALL_DIR设为根.runtime内：

| 实际命令 | 结果 | 日志 |
| --- | --- | --- |
| `uv sync --project tools/mir2x-mcp --locked --group dev` | 锁定安装通过 | `.runtime/collaboration/t06-sync.log` |
| `uv run --project tools/mir2x-mcp --locked --no-sync python -m pytest tools/mir2x-mcp/tests -q --tb=short` | 76/76通过，8.99秒 | `.runtime/collaboration/t06-pytest.log` |
| `uv run --project tools/mir2x-mcp --locked --no-sync ruff check tools/mir2x-mcp` | 通过 | `.runtime/collaboration/t06-ruff-check.log` |
| `uv run --project tools/mir2x-mcp --locked --no-sync ruff format --check tools/mir2x-mcp` | 17个Python文件格式通过 | `.runtime/collaboration/t06-ruff-format.log` |

测试实际包含：官方SDK子进程initialize/tools-list/resources-list/全部35工具成功+失败+extra拒绝/6资源读取/六态返回，真实SDK取消通知，stdin EOF后进程自行exit 0（未强杀），断线写不重发、重连旧context拒绝、多实例与指定实例、实例退出不改绑、陈旧实例、错误token/版本、分片、并发读、超限帧、请求超时、忙碌时stop、心跳；Windows真实受保护ACL、公开ACL拒绝、硬链接拒绝；uint64/UTF-8/重复键/非有限数/深度/严格整数边界。生成检查在临时目录执行，不修改产品源码。

### 限制与审查

- 本次是T06开发交付；TCP服务端为fake，实际C++桥接和真实窗口场景留T07。没有启动游戏或改存档，不能据76项通过宣称AI已可操作角色。
- POSIX权限分支未在Linux/macOS执行；Windows为本次实测平台。
- T06交付时MCP固定工具列表包含尚未完成的C++域适配；此后T02至T05已接入全部35项，宿主仍须读取当前capabilities/策略与阶段。Python不伪造游戏成功，最终全链路实测仍未完成。
- 使用SDK公开MCPServer及list_tools/call_tool覆盖钩子精确发布共享schema；不修改SDK私有管理器。此局部实现细节未改变总计划范围/契约/验收。
- 主会话已按产品源码、共同契约、实际stdout握手和上述日志审查接受T06；执行登记由总计划更新。真实集成仍由T07汇合，不提前进入整体Verification。

- 2026-11-01 08:24:44：补充examples/tool-workflow.json，查询/动作/accepted/业务失败/unknown样例经共享契约校验；仅文档补充，无运行代码修改，76项代码检查结论不变，manifest已更新为23文件。

- 2026-11-01 08:39:44：T02仲裁补充真实客户端只读互通证据，官方SDK stdio→Python→C++安全凭据/认证/session/events/stop通过；C++阶段disconnected、仅4内建能力，关闭独立窗口后实例凭据清理。对应build VENGINEERING-20261101082828，详情.runtime/collaboration/T02-client-smoke-report.md。没有登录或角色动作，实际业务集成仍由T07验证；76项自动测试本身仍使用fake。
