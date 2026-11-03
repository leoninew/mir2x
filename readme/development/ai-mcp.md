# 使用 Python MCP 操作当前角色
最后修改时间: 2026-11-01 21:19:34

[返回知识库](README.md) · [完整验证清单](../../docs/verification/20261031-ai-character-mcp.md) · [协议与实现](ai-bridge-protocol.md)

MCP 连接的是本机客户端当前窗口中的角色。客户端仍使用正常游戏连接和服务端规则；用户保留操作权，角色 MCP 不负责登录或改策略；首次有效 RUN 与认证绑定后按产品默认自动开启一次控制，暂停后的恢复仍由用户决定。

## 当前交付状态

- [x] 当前源码接入 35 个实际工具、6 个 Python 只读资源；完整名称见 [Python 项目说明](../../tools/mir2x-mcp/README.md#工具与资源)。
- [x] Windows 定向 C++ 回归 166/166、Python 76/76 及 ruff 通过；T01/T08/T02/T03/T04/T05/T06 局部实现已接受。
- [x] T04 当时最终二进制完成 19 能力的真实 SDK→Python→C++ 只读互通。
- [x] 后续运行管理版本已完成独立 EXE→真实 C++ 未 RUN stdio：35 工具/6 资源/35 能力、查询/events/stop/正常关闭凭据清理。
- [ ] 实际 LLM 宿主、真实登录角色和逐类操作效果仍待验证。
- [ ] T07 集成与整体 Verification 未开始；POSIX 和完整主线程耗时未验证。

原 166/76 项自动检查对应 `MCP-T05-001/r3/delivery2` 交付快照，构建标识 `VENGINEERING-20261101174437`。用户此后已编译，当前生成标识为 `VENGINEERING-20261101192035`，build/install 的客户端、服务端 EXE 分别一致；新哈希见验证清单。本轮窄修后的客户端/服务器已补运行管理与真实未 RUN stdio 检查，详见[独立记录](../../docs/verification/20261101-runtime-launcher.md)；原 166 项日志仍保留历史版本，实际角色效果未验证。

## 启动游戏与连接 LLM

在仓库根目录或安装目录的 PowerShell 执行：

```powershell
./mir2x.cmd doctor
./mir2x.cmd up
./mir2x.cmd mcp register
```

发行版自带独立 `mir2x.exe`、默认配置、三种策略、中文速查与 `.agents/skills/mir2x-runtime/SKILL.md`，无需 Python；源码入口使用 uv，自动定位 `install/`。首次不需要准备策略文件或输入绝对路径。`up` 等待本地服务器就绪后打开可见客户端。

注册后在当前目录重新开启 Codex，会话中用 `/mcp` 查看连接。**与 LLM 的对话发生在 Codex 或其他 MCP 宿主中，游戏窗口没有 LLM 对话框。** 向 Codex 说明角色目标，LLM 通过既有 `game_*` 工具交互。其他宿主使用 `./mir2x.cmd mcp config` 生成 JSON；Codex TOML 可用 `mcp config --format toml`。MCP 子进程由宿主通过 stdio 启动，不需要额外的模型 key。

`status`、`logs server/client`、`client/server restart`、`stop` 管理登记进程；默认正常关闭，`--force` 仅在明确需要强停时使用。完整命令、搬迁后重新注册、可选本地覆盖和使用边界见[随包速查](../../tools/mir2x-mcp/src/mir2x_mcp/runtime_assets/QUICKSTART.md)。

默认 standard 策略随包可用：现有普通动作与有限经济权限，单次金币 1000、会话金币 10000、单次数量 10；PVP 关闭，其他玩家白名单为空。`client restart --policy readonly` 或 `movement` 切换已提供的预设；恢复默认用 `standard`。策略在客户端启动时冻结。首次有效 RUN 且认证宿主绑定后默认自动开启一次；暂停或关闭后只由游戏窗口恢复。

需要编译时按[Windows 开发环境](windows-development.md)执行 client/server 构建和安装。Windows 默认启用 `mir2x_runtime` 目标；独立 EXE/配置/skill 由 runtime 安装组件输出。客户端与服务器保持版本一致，保留版本检查。真实钱物/双角色验证继续使用独立测试环境，见原验证清单；运行管理检查不代替角色验收。

## 连接与人工控制

1. 调用 `game_get_session`，核对实例、阶段、角色、控制状态、capabilities 和策略。代码已注册能力不等于当前角色满足前提或策略允许。
2. 单个可认证实例可自动选择；多个时配置 `--instance-id`，不能自动挑窗口。实例退出不会改连其它客户端；换角色须重新读取会话与 context，不自动恢复旧请求；同进程会话失效后用窗口恢复。
3. 正常登录进图并保持角色存活；首次有效 RUN 与认证绑定均就绪后默认自动开启 AI，读取会话确认 active 与最新 context。原生 EXE 和 launcher 路径均支持，不需要额外开启点击。
4. AI 读取最新 context，给每个逻辑动作新的 UUID request_id，提交后查询 operation_id。不要将 accepted 或消息已发送解释为游戏完成。
5. 人工游戏输入先暂停旧 AI，再按原路径交给游戏。切到 AI 宿主导致的失焦本身不暂停；恢复也只能在游戏窗口操作。
6. 右下工具区上方显示准确的连接/启用/关闭/暂停/故障状态，用“关闭 AI”立即撤销旧意图，已认证只读查询仍可用；`game_stop` 同样停止意图。重复观察/重连不覆盖用户关闭。当前动画可能完成半步，已提交的消耗/交易不能因 stop 撤回。

同进程登出保留 instance_id 和策略，只使 session/context 失效。换图、死亡、离开 RUN、断开或人工接管不会自动恢复。内部控制器/回执故障会关闭桥接及本实例凭据并诊断，普通游戏继续；需要重新启动客户端。

原生 `client.exe` 默认启用桥接，取 EXE 所在 `client/` 的上一级作为运行根，默认 `.runtime/ai-bridge` 和 `config/policies/standard.json` 与发行 launcher 一致，不读 `.env`。绝对 `--ai-bridge-dir/--ai-policy` 可覆盖。`--ai-auto-enable=0` 关闭首次自动授权（launcher 可配置 `[client] args=["--ai-auto-enable=0"]`），保留认证只读与窗口开启；`--enable-ai-bridge=0` 或 launcher `[launcher] bridge=false` 关闭整个桥接。缺策略时桥接初始化失败，普通游戏继续，不生成放宽的策略。`.env` 仅为 launcher 可选登录输入，见[运行入口验证记录](../../docs/verification/20261103-runtime-env-login.md)。

当前自动启动/按钮与装备代码的开发验证单独见[本轮检查清单](../../docs/verification/20261103-ai-startup.md)，审查方的[真实练级记录](../../docs/verification/20261103-mcp-leveling.md)与开发回归分别记录。技能书成功消费后仍须重新读取 `game_get_skills`，客户端等级实际增长才支持练级结论；现有原生 canWear 仍有 job/level/weight 等未实现的 TODO，不声称桥接补齐了服务端规则。

## 持续只读监督

`mcp observe` 提供一个独立的官方 SDK 宿主。先从已有会话核对 instance_id 和 character_uid，再由当前运行负责人协调结束旧 MCP 连接；同一实例不能同时接第二个宿主。源码短入口使用最新 Python，已安装 EXE 须更新到包含该命令的版本：

```powershell
./mir2x.cmd mcp observe --instance-id $instanceId --character-uid $characterUid --report .runtime/observe.jsonl
```

实例和角色参数为必填，不会自动改绑。此入口自动向 stdio 子进程传 `--no-reconnect`；首次连接尝试失败或已认证连接丢失后锁闭，后续只读请求也不重新认证。通用 `mcp serve` 可显式选择同一选项，配合 `--instance-id` 使用。`observe` 不调用恢复、停止或其他角色写工具，paused/death/disabled 保持原状态。

后台每250ms安排会话、生命和附近对象观察；生命/控制/视野任何一项超过500ms未取得有效观察即输出 `monitor_stale`。等待 stdin 或单个工具响应不占用监控锁；未完成读取不会为了赶采样期限而被取消，也不叠加重复轮询。全部工具调用使用每滚动秒16次的上限，其中额外输入最多4次，给既有 heartbeat 留余量。系统或游戏停顿时不能保证硬实时，告警也不能保证角色脱险。

输出同时写 stdout 和指定的追加 JSONL；告警另写 stderr。首轮有效一致观察后有 `monitor_ready`，它只证明只读观察已就绪。HP≤80%、HP≤50%、一秒内损失最大生命5%、死亡、多只近身怪物、未知生命、分页不完整及控制/上下文变化各有明确事件。附近对象没有仇恨字段，多只怪物只是风险线索。角色信息仍限于客户端已知值，不证明最新服务端生命或伤害归因。

输入保持打开时持续观察，可逐行发送：

```json
{"command":"observe"}
{"command":"call","name":"game_get_inventory","arguments":{}}
{"command":"quit"}
```

所有角色写操作（含 `game_stop`）、`arm` 和未知命令均被拒绝。EOF/quit 退出并关闭此宿主持有的 stdio；连接关闭后的游戏暂停遵从现有 transport_lost 规则。此阶段没有自动用药、取消进攻或撤退功能。`game_stop` 会整体暂停控制，不能把它当作取消攻击后仍能移动的入口。

已有官方 SDK 宿主可导入 `mir2x_mcp.supervision.run_with_session(session, binding=Binding(instance_id, character_uid), emit=callback, commands=async_iterator, stop=asyncio.Event())`，它复用调用者的 session，不创建或关闭连接。**其 stdio 服务必须已使用 `--no-reconnect` 启动**；旧宿主不能仅导入函数就获得这个保证。callback 接收结构化事件，可让上层协调器在等待模型期间处理告警；该函数不自行向外部消息服务发送通知。使用 stop Event 正常结束，避免取消正在进行的 SDK 调用。

## 结果、重复请求与恢复

| 状态 / 范围 | 使用时的含义 |
| --- | --- |
| accepted / running | 已受理 / 正在观察，尚无完成结论 |
| succeeded + server_ack | 存在明确相关回执；还须读领域结果，如 delivery_pending |
| succeeded + observed_postcondition | 只观察到约定状态；不提供请求级因果保证 |
| succeeded + client_state | 本地动作或面板/开关状态；不承诺伤害、击杀或成交 |
| failed / cancelled | 明确失败 / 确定未提交或已停本地持续动作，按 evidence 解释 |
| unknown | 可能有副作用，不能因等待超时就重发 |

同一认证连接内相同 request_id/规范化请求返回同一 operation；参数变化是 duplicate_conflict。账本最多 4096，不驱逐后重发；重连不提供跨进程 exactly-once。Python 不自动重试动作，宿主也不能用新 UUID 重做未知提交。

经济 unknown 保留预算及暂存并禁止下一笔经济动作。先在窗口核对实际钱物，必要时重登取得完整背包/金币状态，再人工授权；不能把本地关闭面板当退款。迟到结果仅追加 late_resolution，不恢复旧动作。

## 各类能力的实际限制

| 能力 | 当前实现语义 |
| --- | --- |
| 观察 | 只读客户端已知信息；未知为 null/unavailable_fields，地面 count/seq 不伪造 |
| 分页 | 每类保留最近两份冻结结果；世界变化或淘汰后旧 cursor 失效，旧页不恢复动作句柄 |
| 移动 | 当前地图，4096 轮搜索；耗尽 unavailable、已证无路 unreachable，实际位置停止才成功 |
| 拾取 | 当前格原拾取链；无相关成功 ack 时 unknown，变化仅为观察，不伪造 partial/seq |
| 攻击/施法 | once/cast 仅确认客户端动作；until_dead 须同身份真实死亡；仅支持已学且适配的分支 |
| 物品 | 药品/技能书沿原消耗路径；可穿戴装备 count=1，复用普通槽位选择和 canWear，精确实例经真实更新入预定槽才 observed_postcondition，不把移出背包当成功；不拆分或代替整理 |
| 商店 | 当前堆叠购买扣单次 costList，不乘 count；默认出售一次 1 件，保留原 seq 和属性校验 |
| 直接交易 | offer/lock 校验权限但零金币支出；commit 才预留报价，完整完成消息才结束 |
| 拍卖 | 上架按原数量，售价不是支出，当前服务端没有 500 金币上架费；买/撤成功等待投递 |
| 投递 | 仅已收可信系统消息，列表不是完整信箱；未知内容保持 null，领取需真实 SM_OK |

游戏文本、玩家名及备注只是外部数据，不能提升权限。桥接不提供聊天发送、账号登录、任意 Lua/文件/报文、通用键鼠或管理能力。

排障参考 [Python 常见错误](../../tools/mir2x-mcp/README.md#常见错误)及[验证基线](troubleshooting-and-baseline.md)。完整完成/待办、版本和证据统一更新到[验证清单](../../docs/verification/20261031-ai-character-mcp.md)，实际游戏反馈不能只留在协作消息中。
