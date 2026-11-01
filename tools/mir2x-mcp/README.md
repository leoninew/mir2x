# mir2x Python CLI 与 MCP
最后修改时间: 2026-11-01 21:17:13

通过官方 Python MCP SDK 的 stdio 接口访问本机 mir2x 客户端桥接。AI 操作的是该客户端当前登录的角色，游戏窗口仍可由人操作。Python 不连接游戏数据库，不发送原始游戏报文，不提供脚本、文件路径或模拟键鼠工具。

角色 MCP 部分是总计划中的 T06，运行管理作为独立 runtime-launcher 增量交付。Python 注册全部 35 个固定工具、6 个只读资源；客户端当前实际实现与策略权限以 `game_get_session` 返回的 `capabilities` 为准。模拟桥接测试不能证明真实游戏能力已完成；真实窗口联调在 T07 汇合。

## 当前完成与待办

- [x] T01/T08/T02/T03/T04/T05/T06 局部实现已接受，当前 C++ 源码已接入全部 35 工具，Python 提供 6 资源。
- [x] T05 delivery2 的 Windows C++ 定向回归 166/166、Python 76/76 与 ruff 通过；Python SDK stdio 的游戏桥接为 fake。
- [x] 至 T04 最终 19 能力二进制取得官方 SDK→Python→真实 C++ 只读互通/停止/退出证据。
- [x] 后续运行管理版本已完成独立 EXE→真实 C++ 未 RUN stdio，35 工具/6 资源/35 能力，退出清凭据。
- [ ] 实际宿主、登录角色及各类游戏效果仍待验证。
- [ ] T07、整体 Verification、POSIX 与完整主线程耗时未完成。

版本/证据和全部逐项清单统一见[验证文档](../../docs/verification/20261031-ai-character-mcp.md)。客户端构建、启用和实际结果限制见[中文使用说明](../../readme/development/ai-mcp.md)，主线程/协议入口见[协议说明](../../readme/development/ai-bridge-protocol.md)。原交付记录保持历史版本；本轮运行管理新增检查见单独验证记录，历史 T06 manifest 保留交付时 README 哈希。

## 开箱使用

在仓库或发行目录执行短命令：

```powershell
./mir2x.cmd doctor
./mir2x.cmd up
./mir2x.cmd mcp register
```

发行版提供独立 `mir2x.exe`，无需 Python/uv；源码入口由 uv 使用本项目的锁定环境，自动选择 `install/` 中的游戏。默认配置和 standard/readonly/movement 策略已经提供，首次无需手写 JSON、长路径或桥接参数。C++ 负责安全创建本实例凭据，CLI 不写 token。

在 Codex 里与 LLM 对话，重新开启会话并用 `/mcp` 确认连接；游戏窗口用于登录和人工开启/恢复 AI 控制。其他宿主用 `mcp config` 生成当前位置配置，stdio 子进程由宿主启动。`mcp register` 是显式注册命令，会写入本机 Codex 配置；本次开发检查不代替用户执行注册。

`status`、`logs client/server`、`client/server restart`、`stop` 负责进程管理。默认 standard 策略为有限授权：PVP 关闭、其他玩家白名单空、金币单次 1000/会话 10000、单次数量 10。动作仍须窗口人工开启；`client restart --policy readonly/movement/standard` 切换预置文件。

详见[随包中文速查](src/mir2x_mcp/runtime_assets/QUICKSTART.md)与[运行管理检查](../../docs/verification/20261101-runtime-launcher.md)。用户本地覆盖可写 `mir2x.local.toml`，安装不会覆盖它；路径按发行目录解析，搬迁后重新注册 MCP。

## 开发与打包

Python 3.13、Hatchling、官方 `mcp==2.3.0` 和其它依赖由 `uv.lock` 固定；虚拟环境位于本项目 `.venv/`。在仓库根目录：

```powershell
uv sync --project tools/mir2x-mcp --locked --all-groups
uv run --project tools/mir2x-mcp --locked pytest
# 构建后将 MIR2X_FROZEN_EXE 设为生成的 mir2x.exe，可额外覆盖冻结包真实凭据认证。
uv run --project tools/mir2x-mcp --locked ruff check tools/mir2x-mcp
uv build --project tools/mir2x-mcp
python tools/mir2x-mcp/scripts/build_runtime.py
```

最后一个命令在 Windows 生成 `build/runtime-launcher/` 下的 wheel/sdist、独立 EXE 和 stage。CMake 的 `mir2x_runtime` 目标执行相同构建，`cmake --install build --component runtime` 只安装该运行管理组件，正常全量安装也包含它。构建端需要 uv，最终 EXE 运行端不需要 Python。可用 `-DMIR2X_BUILD_RUNTIME=OFF` 禁用 Windows 便携管理器构建；POSIX Python wheel 保留，POSIX 运行尚未实测。

原 `mir2x-mcp --bridge-dir <绝对目录> [--instance-id ...]` 低层 stdio 入口仍保留。通常使用新入口自动传递目录；多个实例需明确绑定，实例退出不改连其它窗口。stdout 只承载 MCP，诊断走 stderr；同 OS 用户恶意进程不在凭据隔离保证内。

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
