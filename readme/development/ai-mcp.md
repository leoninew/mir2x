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

## 带安全计划的监督

`observe` 始终只读。需要由同一宿主安排普通短攻击、用药和撤退时，使用单独的短入口；先由运行负责人结束旧宿主，不能连接第二个 MCP 会话：

```powershell
./mir2x.cmd mcp supervise --instance-id $instanceId --character-uid $characterUid --report .runtime/safety.jsonl
```

该命令复用同一运行根/配置，强制 `--no-reconnect`，不恢复游戏控制。开始时只观察；角色必须已经通过正常窗口进入 alive/active，生命、会话和附近对象都在500ms内有一致观察。原有 SDK 宿主可在同一个 `run_with_session` 调用中传 `allow_actions=True`，仍由调用者持有唯一 session；所有本轮角色写入交给该监督器，不再并开外部写循环。

监督器提供可复用的公开工具调度，不固定角色、地图、购买物品或练级次数。武装前根据调用者的任务准备物品并重新读背包确认储备。HP 药品语义来自原物品表的金创药小/中/大/特（item_id 6/7/8/9），同时核对规范名称、调用者选中的精确 seq_id、inventory 位置和未被交易等界面保留的数量；不从名字推断类型，不把装备/腰带当作可消费库存。分页不完整或库存未知会拒绝准备。

准备购买使用显式 `purchase`，不要求已有药品。普通 NPC 界面已打开后，传本轮实读的完整 context、shop_handle、item_id、本笔 max_total_gold 和新 intent_id；示例中的省略号必须替换，epoch 和金币为十进制字符串。每笔 count 固定为1，保留原客户端已知的批量报价限制；物品、价格和次数由调用者任务决定，每笔都是新的显式意图，不自主补货或循环购买。总支出同时受原生冻结策略的单笔/会话上限、item/action 权限及剩余金币预算约束，宿主不扩大策略。下面 item6/80 仅为这次小药验证输入，模块没有这些常量限制：

```json
{"command":"purchase","context":{"instance_id":"...","session_id":"...","world_epoch":"1","control_epoch":"3"},"shop_handle":"本轮商店句柄","item_id":6,"count":1,"max_total_gold":"80","intent_id":"00000000-0000-4000-8000-000000000001"}
```

它只在 disarmed、无待决动作、新鲜已知 alive/active、停住、完整视野无 monster 时执行；可在安全城镇低血时准备补给，不套战斗任务阈值。公开 session 的原生经济策略必须可读且无 economic_uncertain。唯一调度器在最多8秒内有界读取指定商店和物品，等待真实刷新；只接受唯一匹配的 buy、unit_count1、无材料、余额、本笔上限和原生预算允许且未接近过期的报价。同一执行内随即发一次公开 `game_shop_buy`，不把 quote 交回 LLM 等下一轮，不延长原30秒 TTL，不猜句柄。所有读取和写入仍受总调用预算限制，监督不等待输入或交易完成。

原生经济摘要的真实路径是 `game_get_session.data.capabilities.policy`。商店存在多个同 item_id 实例或材料交换时，本便捷命令会拒绝歧义，不替调用者猜实例。原有公开 `game_shop_list` / `game_shop_buy` 仍可用于按真实 quote_handle 选择；须先停止本监督器写调度并确认没有未决/未知操作，再由同一 caller-owned session 的唯一写者执行，不能并开第二连接或重做未知购买。

操作终态后重新读背包与金币，最多等2秒确认单件增加与实际报价金额扣除；`purchase_result` 分开给出 operation、inventory_before/after、gold_before/after、delivery_observed 和 purchase_confirmed。accepted 或 server_ack 单独不证明已交付，更不证明治疗；`healing_attributed` 始终 false。busy/stale_target 不重试，重复 intent 拒绝，未知或响应丢失锁闭写入，已受理但交付未确认也锁闭后续自动写入；暂停、怪物进入视野或上下文变化可使流程提前终止并告警，不能续买。换连接不能被用来刷新支出预算或重做未知购买。

成功终态的受理事实先保存为 `unconfirmed_purchase`，仅在库存与金币同时确认后清除。复读错误、分页/库存不完整、过期或暂停导致流程失效时，该事实仍强制锁闭本轮后续写入；普通异常清理不会丢掉它。`observe` 的 status 独立输出原 operation/request_id、报价和库存/金币基线，不能以新的 intent 绕过未确认交付。

退路必须有本宿主实际观察的行走证据。`trail` 输出本地图最近120秒的已走边：普通单步 walk / mounted walk 的运动和停住终点，或本调度器真实 `game_move_to` 的 submitted、observed_postcondition 及后续新鲜停住观察。相邻静止坐标、猜测坐标和导入旧日志均不能制造一条已走边。

无需第二写者即可准备退路：明确发送 `scout`，提供当前 context/map_uid、选中药品实例、一个相邻目标格及显式 safety 任务数据。`reserve_doses` 是要求的可用剂量；`low_hp_percent` 为停止进攻阈值；`rapid_loss_percent`/`rapid_loss_window_ms` 为掉血判断。它们不隐式取这次验证角色的值。reserve 范围1..uint32，低血百分比1..99、快速损失百分比1..100、时间窗250..10000ms；缺字段或越界拒绝。任务储备、健康及当前地形/动态占用/附近对象均通过后，最多发一个2000ms普通单步移动；成功且确认停住后只登记该段，不启用狩猎。下面3剂量/80%/5%/1秒及坐标仅为调用者示例，须替换为本轮任务和实读值：

```json
{"command":"scout","context":{"instance_id":"...","session_id":"...","world_epoch":"...","control_epoch":"..."},"map_uid":"...","potions":[{"item_id":6,"seq_id":1}],"safety":{"reserve_doses":3,"low_hp_percent":80,"rapid_loss_percent":5,"rapid_loss_window_ms":1000},"destination":{"x":2,"y":1}}
{"command":"trail"}
{"command":"arm","context":{"instance_id":"...","session_id":"...","world_epoch":"...","control_epoch":"..."},"map_uid":"...","potions":[{"item_id":6,"seq_id":1}],"safety":{"reserve_doses":3,"low_hp_percent":80,"rapid_loss_percent":5,"rapid_loss_window_ms":1000},"retreat":[{"x":2,"y":1},{"x":1,"y":1}]}
{"command":"attack","target_handle":"本轮新鲜目标句柄","intent_id":"00000000-0000-4000-8000-000000000010"}
```

`arm` 的路线以当前实际位置开头，包含1至4个相邻、已走过的短段。武装、每次进攻和每段撤退都会重新检查本地图 walkable、明确的动态空闲、斜走转角，以及目标路点一格内没有已知存活怪物/玩家/NPC；未知或不完整视野会拒绝。它们是当前客户端观察下的候选退路，不保证怪物不移动，也不证明脱战。角色手工行走的历史可跨同地图 control_epoch 保留，但任何现有计划在暂停或 epoch 变化时立即失效，必须重新明确武装。

每条 `attack` 只接受当前相邻、已知存活的 monster，固定 `mode=once`、timeout=2000ms；拒绝 PVP、远距追击及 until_dead。`intent_id` 必须是本轮新 UUID，同一意图最多尝试一次。已知结束后下一击需要新的目标观察和新的明确意图；没有后台续击。一次 client_state 完成只表示普通本地攻击动作完成，不能据此写成伤害、击杀、经验或升级。

达到调用者 safety 低血或快速掉血阈值会锁住狩猎，即使 stdin/LLM 空闲也触发。唯一写调度器优先精确 count=1 用选中药品，再逐段检查并走已验证退路；进行中的短操作须先取得可用终态。药品库存后置条件、消费确认、后续实际HP增长分别记录，HP增长不自动归因于药品。busy 不自动重试；已知未提交的用药拒绝可在计划仍有效时尝试退路，未确认/未知移动不能续段。血量回升和撤退完成都不自动恢复狩猎。原只读 Observer 的80%/50%/1秒5%是告警阈值，不能代替这些显式任务条件，也不触发角色写入。

任何 unknown、动作响应丢失、操作身份不匹配或本地2.5秒后仍无确定结果，都会永久锁住本轮后续自动写入，保留 request_id/operation_id 和原始结果，只读监督继续；不会重放，也不能再承诺自动撤退。paused/manual/death、context/map/身份变化、过期观察、EOF 会撤销计划及未发送命令，旧计划不自动恢复。准备和写入共享每秒16次总预算；额外只读 call 在武装期间拒绝，避免抢占药品/操作查询余量。

`disarm` 解除计划。若已有未决动作，或明确输入 `stop`，调度器在能确认当前绑定时尝试一次公开 `game_stop`；它是全局暂停，随后绝不试发药品/撤退。响应仍未知时只告警并锁闭，不能假装已安全停止。`quit`/EOF 同样撤销计划；独立 CLI 退出关闭自己的 stdio，模块接口不关闭调用者 session。系统停顿、工具无响应、暂停或怪物继续攻击时，宿主只能记录 `no_safe_action` / 不确定性并及时交由窗口处理，不能使角色无敌。

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
