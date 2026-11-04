# mir2x 当前角色桥接契约 v1
最后修改时间: 2026-11-01 19:26:21

本目录是 T01 的跨语言契约。它定义 37 个公开工具以及内部 `hello`、`heartbeat` 方法。T02 至 T05 已完成客户端调度及全部域适配，本目录和传输库本身不操作角色。当前状态和全部验证缺口见[验证清单](../../../docs/verification/20261031-ai-character-mcp.md)，主线程/领域接线见[协议与实现说明](../../../readme/development/ai-bridge-protocol.md)。本版授权与商店关闭同步更新 schema/fixture；历史 manifest 保留。

## 文件与复现

- `contract.schema.json`：JSON Schema 2020-12，根引用 `request`；`$defs` 包含每个工具的 `Request`/`Response`、操作、快照、事件、策略、实例和错误结构。
- `fixtures.json`：每个工具的合法请求、成功响应、失败响应和非法参数样例。均为脱敏示例，不含可用认证信息。
- `operation-fixtures.json`：六类操作状态示例。成功结果必须有非空 evidence 和 completion_scope，不能以发送成功替代游戏完成。
- `error-fixtures.json`：稳定公开错误码的响应示例。
- `policy-readonly.json` 与 `policy-example.json`：默认拒绝策略和有有限金额/数量的完整动作示例。玩家对手列表默认空，真实测试须显式配置对手。NPC 脚本权限独立，不承诺交易预算覆盖脚本的所有副作用。
- `generate.py`：仅使用 Python 标准库的确定性生成器；修改契约时编辑此文件并运行 `python contracts/ai-bridge/v1/generate.py`。生成文件随源码交付，运行游戏和 MCP 不需要执行生成器。

CMake 在配置阶段将同一 JSON 嵌入 `mir2x_ai_bridge`，并监测契约文件变化。C++ 校验使用此内嵌内容；Python 必须使用相同版本及 fixture，不能另造私有字段。`x-methods` 列出每个方法的参数、结果定义、中文含义和动作分类。

## 字节与类型

每帧为 4 字节大端无符号长度，加 UTF-8 JSON。请求体最多 65,536 字节、响应体最多 1,048,576 字节；0 长度与超限长度在分配正文缓冲之前拒绝。JSON 最多嵌套 16 层，禁止重复键（含嵌套对象）、无效 UTF-8、实际 NUL 字节、非有限数字和尾随非空白内容。

所有对象均封闭。整数参数拒绝浮点表示、负数和越界；UID、金币、计数代次和时间戳使用十进制字符串，不接受前导零、正负号、小数或指数。时间戳是 Unix epoch 毫秒。`x-decimalMaximum` 与 `x-maxUtf8Bytes` 是必须额外执行的约束：普通 JSON Schema 库可能忽略它们，消费者必须检查字符串表示的整数上限和 UTF-8 字节长度。C++ 已执行两者。Schema 的字符串长度仍按 Unicode 码点计算。

方向沿用游戏 `DIR_BEGIN=1` 至 `DIR_UPLEFT=8`；未知是 null。背包物品有精确 seq_id/count/location/预留数量；地面物品允许 seq_id/count 为 null。交易物品保留已知实例与数量；拍卖和投递使用独立公共详情，不能标成背包。投递 items=null 表示未知内容，items=[] 才表示已知为空。

`count` 的一般上限取 GUI 可表示的 INT_MAX；use_item 使用 CMConsumeItem 的 uint16 上限 65,535，具体技能书等单次语义由动作适配器进一步校验。拍卖分类沿用 1 至 11；备注最多 128 UTF-8 字节。格子坐标限非负 INT_MAX，实际地图边界由 T03 检查。

## 请求、控制与结果

`game_grant_control` 是同步控制写入，不是查询或异步游戏动作。params 必须含
`mode` (`new_task` / `continue_task`)、UUID `grant_id`、UUID `request_id`、
十进制 `character_uid` 和完整 `context`；成功返回 grant_id/mode/active/新 context，
不产生 operation 或游戏发送。SDK 必须重新读取 session，再提交新动作。
宿主将真实用户输入事件 user_intent_id 稳定映射到一个 grant_id；重复输入、旧 goal、
轮询和重试不能生成新授权。原生只验证认证、身份、context、policy 及有界去重账本，
不能凭 UUID 证明对话来源。新授权响应丢失不重试。

普通人工输入停止 intent、递增 epoch，但有效任务保持 active；原动作和计划不恢复。
同角色同 session 的正常加载、换图和生命恢复可在重新观察后 continue_task；加载期间
context 为 null，死亡/未知生命不授权。off/默认 game_stop 锁存停止，远端解除必须新用户指令及新 grant。
用户在真实窗口明确开启也是新的本地授权，保留生命/认证/故障/手势检查，撤销旧 grant，
不恢复旧队列或计划；旧 grant 仍不能 continue_task。普通人工输入保留有效本地授权。
故障、失联或换角色/session 不续用旧授权；授权不清 unknown、未确认购买或预算。
write 元数据明确标识 grant/stop；observer 和资源只允许 write=false 的方法。

`game_shop_close(shop_handle)` 沿用动作 envelope；完成只证明本地 client_state，
没有网络发送，也不证明购买撤销/退款。它不改变 game_npc_close 的对话关闭范围。
两项方法须由冻结 policy 允许；新 standard 允许二者，movement 只允许 grant 和移动，
readonly 不允许。旧固定 schema/工具集消费者须和 native/SDK/宿主说明同步升级。

统一请求包含 v/id/method/params。id 是不超过 64 UTF-8 字节的非空字符串，仅用于匹配一次 IPC 调用。动作另含 UUID request_id、context 和 timeout_ms。默认 `game_stop` 是优先控制请求，不要求 context/request_id，保证旧控制代次或账本满时仍能停止；它仍需有效认证。

context 包含 instance_id/session_id/world_epoch/control_epoch。实例和 session 为 32 位小写十六进制；epoch 为 uint64 十进制字符串。T02 在入队和执行前检查上下文，维护 4,096 条连接内去重账本。`canonicalRequest` 返回按键排序、去掉 id 并补齐参数默认值的 JSON，可直接作为精确去重比较值；不依赖非加密哈希的碰撞假设。stop 不进入动作账本。

查询直接返回当前快照或明确错误。动作返回 operation_id 和状态，用 game_get_operation 查询后续结果。shop_quote_sell 会改变当前 INVOP 的报价交互状态，因此归入动作；完成时在 operation.result.quote 返回报价及 quote_handle。商店列表和拍卖刷新为查询，使用 refresh_pending 和 last_query_error 表示异步刷新。

操作状态为 accepted/running/succeeded/failed/cancelled/unknown。只有 succeeded 可以有完成范围，且必须提供非空证据；failed 必须含稳定错误。scope 为 server_ack、observed_postcondition 或 client_state。unknown 保留 side_effects_possible/cancel_requested，迟到结果只能附在 late_resolution；不自动重试或发起后续动作。operation.result 是闭合的域结果字段集合，没有任意报文、XML、脚本或代码入口。

普通错误响应有 v/id/ok=false/error/context。`transportError` 允许 id=null（请求无法安全解码时）；已知业务响应仍须回传原 id。错误只含稳定 code、中文 message 和封闭 details，原始异常、令牌和输入文本不得进入错误。未登录时 context=null，game_get_session 仍可报告阶段。

## C++ 传输接入

`AiBridgeTransport` 只允许主线程调用，不创建线程、不访问游戏对象。未调用 start 时没有监听或凭据。T02 应仅在显式 `--enable-ai-bridge` 下调用 start，并提供绝对 directory/policyPath 与 build。先加载冻结策略，绑定 127.0.0.1:0，再安全创建实例文件；失败关闭监听。CLI 已要求启用时两个路径必填且为绝对路径。

- `pollBudget()`：每轮最多 16 个 ASIO handler，默认软预算 2ms；不等待 socket。单 handler 的帧/JSON/输出都受上限约束，2ms 不是硬实时保证。
- `takeRequests()`：最多返回 8 个请求，已收到的 stop 位于首位。普通队列最多 64 项，stop 单独一个槽；额外 stop 返回 busy，已排队的 stop 保留。
- `sendResponse(connectionId, response)`：仅向仍有效的同一认证连接发送，限制单响应及 2MiB 出站缓冲；慢读或超额会断开。旧连接的结果不能发到新连接。
- `takeDisconnected()`：返回需要由 T02 暂停的连接。断开时丢弃未交付队列；在通知被消费前不允许新连接认证，避免漏掉暂停事件。
- `instance()` / `policy()`：仅用于本地适配；instance 含令牌，不能整体返回 MCP、写日志或嵌入能力摘要。
- `close()`：幂等取消 socket、停止传输并只清理本实例文件。退出路径须显式调用，不依赖当前游戏全局对象析构。

首帧 hello 传实例 ID 和 64 位小写十六进制令牌（256 随机位）。最多 4 个未认证连接、1 个认证连接；未认证超时 3 秒。认证后 heartbeat 每 2 秒；连续 6 秒无有效活动断开。任何出站写持续 6 秒未完成也断开，即便仍收到心跳。每连接每秒最多 20 个普通有效请求；停止绕过此限额。未认证失败直接关闭，不回显秘密。

认证只建立传输信任，不授予游戏动作控制权。T02 必须检查人工开启、策略、生命周期、预算与上下文。默认禁用桥接；本机同一 OS 用户可读取其凭据，本设计不隔离同一用户的恶意进程。

## 验证入口与边界

MSYS2 UCRT64：

```sh
cmake --build build --target test_ai_bridge_protocol test_ai_bridge_security --parallel 8
ctest --test-dir build -R '^ai_bridge_(protocol|security)/' --output-on-failure
```

依赖/源文件变化须先按仓库 Windows 构建环境重配置；CC=gcc、CXX=g++、现有 vcpkg chainload 和 binary cache 保持一致。测试临时目录仅为 `.runtime/test-ai-bridge/`。协议测试使用真实环回 TCP；安全测试使用真实 Windows ACL、AccessCheck 与 junction。Linux POSIX 分支需在 Linux CI 或开发环境另行执行，不以 Windows 结果代替。

实际角色观察、人工接管、经济预算和游戏回执的实现/自动检查已由 T02 至 T06 分别交付；真实 stdio 只读冒烟已取得至 T04 当时最终二进制的证据。T05 最终 35 能力 stdio、真实角色与 GUI 端到端仍待 T07，不由 T01 测试通过推断。

`game_stop` 的兼容内部取消分支要求 `params={interrupt_only:true,operation_id,context}`。必须来自原认证连接、grant模式策略和精确当前context；只取消匹配的当前owned操作。取消递增control_epoch，保持control_state/grant/latch/pause_reason；匹配终态仅报告，不再次取消或停止新操作。缺少字段、旧context或错误操作不会改变状态。不发送idle全局stop。原`{}`及旧operation_id参数仍明确撤销授权/全局暂停；它们使用独立优先槽。内部取消进入普通有界队列，但可见匹配取消在动作advance前处理。unknown/经济预留/未确认交付绝不清除；只有真实取消证据和新鲜观察允许新计划。此分支需成套升级native/契约/SDK，旧运行通道不会自动获得权限。
