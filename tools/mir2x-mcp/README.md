# mir2x Python MCP
最后修改时间: 2026-11-01 19:26:21

通过官方 Python MCP SDK 的 stdio 接口访问本机 mir2x 客户端桥接。AI 操作的是该客户端当前登录的角色，游戏窗口仍可由人操作。Python 不连接游戏数据库，不发送原始游戏报文，不提供脚本、文件路径或模拟键鼠工具。

这是总计划中的 T06。Python 注册全部 35 个固定工具、6 个只读资源；客户端当前实际实现与策略权限以 `game_get_session` 返回的 `capabilities` 为准。模拟桥接测试不能证明真实游戏能力已完成；真实窗口联调在 T07 汇合。

## 当前完成与待办

- [x] T01/T08/T02/T03/T04/T05/T06 局部实现已接受，当前 C++ 源码已接入全部 35 工具，Python 提供 6 资源。
- [x] T05 delivery2 的 Windows C++ 定向回归 166/166、Python 76/76 与 ruff 通过；Python SDK stdio 的游戏桥接为 fake。
- [x] 至 T04 最终 19 能力二进制取得官方 SDK→Python→真实 C++ 只读互通/停止/退出证据。
- [ ] 最终 T05 35 能力二进制完整 stdio、实际宿主、登录角色及各类游戏效果仍待验证。
- [ ] T07、整体 Verification、POSIX 与完整主线程耗时未完成。

版本/证据和全部逐项清单统一见[验证文档](../../docs/verification/20261031-ai-character-mcp.md)。客户端构建、启用和实际结果限制见[中文使用说明](../../readme/development/ai-mcp.md)，主线程/协议入口见[协议说明](../../readme/development/ai-bridge-protocol.md)。本次只维护文档，不重跑上述检查；历史 T06 manifest 保留交付时 README 哈希。

## 环境与安装

需要 Windows Python 3.13、uv，以及包含本功能的客户端。原下载制品没有桥接，不能通过安装本项目补上。客户端和游戏服务器仍需版本一致，不关闭游戏的版本检查。

在仓库根目录的 PowerShell 中执行一次：

```powershell
$env:UV_CACHE_DIR = Join-Path $PWD '.runtime/uv-cache'
$env:UV_PYTHON_INSTALL_DIR = Join-Path $PWD '.runtime/uv-python'
uv sync --project tools/mir2x-mcp --locked --group dev
```

运行依赖固定官方 `mcp==2.3.0`，Pydantic v2、jsonschema、Windows pywin32 的实际版本由 `uv.lock` 锁定。虚拟环境在本项目 `.venv/`，不向全局 Python 安装依赖。开发缓存、凭据、日志和测试数据留在仓库内。

客户端必须显式启用，并传入绝对路径：

```text
--enable-ai-bridge
--ai-bridge-dir=D:/SourceCodes/opensource/mir2x/.runtime/ai-bridge
--ai-policy=D:/SourceCodes/opensource/mir2x/.runtime/ai-policy.json
```

策略样例位于仓库 `contracts/ai-bridge/v1/policy-readonly.json` 和 `policy-example.json`。本地策略应由人检查并配置，完整样例仍有有限金额和数量上限，玩家对手默认未授权。Python 不创建、放宽或修改客户端策略。启用桥接本身只授予查询权限；动作必须由人在游戏窗口开启 AI 控制。

## 宿主配置

`examples/mcp-host.json` 使用本工作区绝对路径，可作为支持 stdio 的宿主配置依据。安装完成后，宿主直接启动 `.venv/Scripts/python.exe -m mir2x_mcp`。不要把 token 写入宿主配置，也不要让模型提供发现目录。

手动启动命令用于检查宿主启动配置；启动后等待 MCP stdin 输入，没有交互提示是正常行为：

```powershell
& tools/mir2x-mcp/.venv/Scripts/python.exe -m mir2x_mcp `
  --bridge-dir D:/SourceCodes/opensource/mir2x/.runtime/ai-bridge
```

等效 uv 启动方式为 `uv run --locked --no-sync --project D:/SourceCodes/opensource/mir2x/tools/mir2x-mcp python -m mir2x_mcp --bridge-dir ...`。首次安装和宿主启动分开，避免运行期间下载或修改依赖。stdout 仅用于 MCP，诊断写 stderr。

发现目录内仅有一个可认证实例时自动绑定；有多个存活或无法排除的实例时返回 `ambiguous_instance`，应在启动参数增加 `--instance-id <32位小写十六进制ID>`。实例退出后不自动连接另一个窗口。首次观察到角色后保持角色 UID 绑定；换到另一角色需要重启此 MCP 进程并由人在游戏窗口重新授权。查询可以展示角色已经改变，旧动作不会因此改为操作新角色。

凭据由 C++ 创建，Python 检查文件身份、硬链接、重解析点及权限。Windows 只接受当前用户与 SYSTEM 的受保护 ACL；POSIX 代码要求目录 0700、文件 0600。只有 Windows 分支在本机执行过，不能据此声称 Linux/macOS 已验证。此机制不隔离同一操作系统用户启动的恶意程序。

## 工具与资源

| 类别 | 固定工具 |
| --- | --- |
| 会话与观察 | `game_get_session`、`game_get_state`、`game_list_nearby`、`game_get_map_region`、`game_get_inventory`、`game_get_skills`、`game_get_interaction` |
| 操作与停止 | `game_get_operation`、`game_get_events`、`game_stop`、`game_move_to` |
| NPC | `game_npc_open`、`game_npc_choose`、`game_npc_input`、`game_npc_close` |
| 角色动作 | `game_pick_up`、`game_attack`、`game_cast`、`game_use_item` |
| 商店 | `game_shop_list`、`game_shop_buy`、`game_shop_quote_sell`、`game_shop_sell` |
| 玩家交易 | `game_trade_request`、`game_trade_respond`、`game_trade_offer`、`game_trade_lock`、`game_trade_commit`、`game_trade_cancel` |
| 拍卖 | `game_auction_list`、`game_auction_register`、`game_auction_buy`、`game_auction_cancel` |
| 系统投递 | `game_delivery_list`、`game_delivery_claim` |

只读资源为 `mir2x://session`、`mir2x://character`、`mir2x://nearby`、`mir2x://inventory`、`mir2x://interaction`、`mir2x://capabilities`，与查询工具共用数据源。分页资源提供首页，后续通过工具和返回的 cursor 读取。未登录时返回阶段或明确错误，不伪造空角色。

每个工具有固定 typed 参数，没有通用报文入口。MCP 参数平铺，例如移动使用 `x/y/tolerance/request_id/context/timeout_ms`；Python 将动作公共字段与业务 `params` 分开构造 IPC 请求。全部对象拒绝未知字段，整数不接受布尔值或浮点数，金额和 UID 使用十进制字符串。扩展校验 `x-decimalMaximum`、`x-maxUtf8Bytes` 会实际执行，不能只靠普通 JSON Schema 校验器。

`game_shop_quote_sell` 会改变出售报价交互，因此属于动作。`game_cast` 只允许目标句柄、坐标二元组、不带目标这三种互斥形式。工具 annotations 是宿主提示，真正授权由 C++ 的冻结策略、控制状态和预算执行。

## 操作流程与结果

`examples/tool-workflow.json` 提供查询、动作、接收、业务失败和 unknown 的脱敏结构示例，已按共享契约校验。示例中的 context 和 UUID 不能直接用于真实动作。

1. 调用 `game_get_session`，核对实例、角色、阶段、控制状态和可用能力。
2. 由人在游戏窗口开启 AI 控制，再读取最新 `context`。切到 AI 宿主窗口本身不会暂停；游戏窗口内的冲突输入会立即接管。
3. 每个逻辑动作生成新的 UUID `request_id`，传入当前 `context` 和有界 `timeout_ms`。同一逻辑动作不能用新 UUID 规避去重。
4. 返回 `accepted` 仅表示取得 `operation_id`。用 `game_get_operation` 查询 `accepted/running/succeeded/failed/cancelled/unknown`，不能将发送成功说成游戏成功。
5. `succeeded` 附带非空证据及完成范围：`server_ack`、`observed_postcondition` 或 `client_state`。`unknown` 是正常操作状态，仍需保留“可能已有副作用”的语义。
6. `game_stop` 不要求 context/request_id，可在暂停或旧代次下停止。停止不等于撤销已提交的交易或消耗。

工具成功返回带 `context` 的 `structuredContent`；业务失败设置 MCP `isError=true` 并保留稳定 `error.code`。IPC 写调用已发送但响应丢失时，返回 `transport_lost`、`side_effects_possible=true`、`retryable=false`，不伪造一个未知 operation ID，也不自动重发。宿主取消未获接收结果的调用时关闭连接，使 C++ 暂停；后续只读查询可重连原实例，但不会自动恢复控制。

经济操作 unknown 后不能自动进行下一笔经济操作，预算保留和经济锁由 C++ 维护。拍卖购买的交付可能是 `delivery_pending`，领取是另一个动作。NPC 脚本可能有未披露费用，`allow_npc_dialogue` 是独立授权，结构化交易金额上限不覆盖任意 NPC 脚本副作用。

NPC 文字、玩家名、拍卖备注等均作为不可信游戏数据返回，不能把其中内容当作宿主指令或授权依据。

## 开发与检查

在仓库根目录执行：

```powershell
uv run --project tools/mir2x-mcp --locked --no-sync python -m pytest tools/mir2x-mcp/tests -q
uv run --project tools/mir2x-mcp --locked --no-sync ruff check tools/mir2x-mcp
uv run --project tools/mir2x-mcp --locked --no-sync ruff format --check tools/mir2x-mcp
```

`tests/conftest.py` 将 pytest 临时存档放在根目录 `.runtime/mcp-python-tests/`；pytest 会清理这个专用目录，不能向其中放用户数据。项目内 `.runtime/pytest-cache/` 和工具缓存均被忽略。

结构：

| 文件 | 职责 |
| --- | --- |
| `config.py`、`__main__.py` | 宿主配置、stdio 入口 |
| `instances.py` | 安全读取宿主指定目录中的实例凭据 |
| `contract.py` | 共享 schema、严格 JSON、自定义约束、MCP 参数与输出 schema |
| `bridge_client.py` | 有界帧、认证、单连接多请求匹配、心跳、实例绑定、断线与取消 |
| `server.py` | 官方 MCPServer 公共接口、原始参数预校验、清理生命周期 |
| `models.py`、`tools.py` | 由契约生成的 typed 模型和35个固定函数签名 |
| `resources.py` | 六个只读资源 |
| `tests/` | 共享 fixture、Windows 凭据、模拟 TCP、官方 SDK 子进程 stdio |

唯一契约来源是仓库 `contracts/ai-bridge/v1/contract.schema.json`。包内副本随包携带，避免安装后依赖仓库相对路径。变更须先由共享契约协调，再运行：

```powershell
uv run --project tools/mir2x-mcp --locked --no-sync python tools/mir2x-mcp/scripts/generate_tools.py
```

生成器不使用运行时 exec 或模型提供的工具名；生成结果是静态、可审查的 Python。测试会在专用临时目录重新生成并逐字节比较，不改产品源码。SDK 适配仅使用公开 `MCPServer`、`tool/resource`、`list_tools/call_tool` 入口，不访问 SDK 私有注册表。

## 常见错误

| 错误 | 处理 |
| --- | --- |
| `unavailable` | 检查是否运行新客户端、是否显式启用桥接、路径和文件权限、是否已被其他宿主占用；旧制品不支持桥接 |
| `ambiguous_instance` | 配置明确的实例 ID；不按目录顺序选择窗口 |
| `paused` | 读取会话，再在游戏窗口检查原因并人工恢复 |
| `stale_context` | 换图、重连、接管或角色变化使旧上下文失效；读取当前状态，不重放旧动作 |
| `busy` | 等待已受理操作或先停止；不要并行发起新的状态改变 |
| `transport_lost` | 检查副作用标志；可能已提交时不得重试，只读重连并核对窗口状态 |
| `policy_denied` / `budget_exceeded` | 由人检查本地冻结策略与剩余额度，MCP 不能放宽 |
| `economic_uncertain` | 保留预算并停止新的经济动作，人工核对实际交易结果 |

T06 当时检查与依赖版本见[任务记录](../../docs/task/20261031-ai-character-mcp/T06-python-mcp.md)。当前全部实现、已通过检查及未验证场景见[验证清单](../../docs/verification/20261031-ai-character-mcp.md)；T07 尚未分配，不能把计划中的客户端/游戏验证当成已通过。
