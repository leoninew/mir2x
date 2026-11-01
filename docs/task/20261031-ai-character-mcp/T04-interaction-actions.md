# 子任务 T04：NPC、拾取、战斗与物品使用
最后修改时间: 2026-11-01 19:26:21

## 审查状态

方案 Accepted；`MCP-T04-001/r3/delivery1` 已于 2026-11-01 10:08:36 由主会话 accepted，沿用总计划修订 3。

- [x] NPC/角色动作实际接入、116 项交付回归及最终二进制 19 能力真实只读 stdio 冒烟已接受。
- [ ] 真实 NPC/拾取/攻击/施法/消费正常和失败路径仍待验证，见[验证清单](../../verification/20261031-ai-character-mcp.md)。

下文“等待主会话审查/接受后再分配 T05”保留交付时快照，T05 此后已接受；本次不重开任务或更新历史 manifest。实际 pickup 没有相关成功 ack，unknown 边界继续有效。

## 父任务与权威记录

- [总意图](../../intent/20261031-ai-character-mcp.md)、[权威总计划](../../plan/20261031-ai-character-mcp.md)。任务 T04，协调根 `D:\SourceCodes\opensource\mir2x`。

## 目标与验收

通过现有正常动作实现 NPC 对话、拾取、普通攻击/施法和支持的物品使用，覆盖 A02、A03、A04、A05、A08。每类能力至少有正常、明确失败和无法确认结果的路径。

## 范围与文件

- `client/src/aibridge/actions.*` 和对话状态适配。
- `processrun.cpp/.hpp`、`processrunnet.cpp`、`myhero.cpp/.hpp` 的窄动作入口/结果事件。
- `gui/npcchatboard/`、输入面板、`gui/skillboard/`，仅共享当前交互和技能使用语义。
- 扩充 `test_ai_bridge_actions.cpp`；不新增服务端协议、调试入口、自动任务执行或装备管理。

## 输入

直接前置 T03 已接受，包含对象/地面/物品句柄、观察与移动路径；其基线含 T02 的状态机和停止规则。schema 中所有本任务工具在 T01 已确定。

## 局部技术设计

### NPC

- `game_npc_open` 校验当前可交互 NPC，然后复用 sendNPCEvent 的 SYS_ENTER。它不隐式移动角色；距离不符明确提示由宿主先移动。
- 从实际加载的 SDNPCXMLLayout/现有 tinyxml2 解析结果抽取显示文本与选项，句柄内部保留 path、id、args、autoClose。MCP 只传句柄；不接受任意原始 XML、path/id 或 Lua 名称。
- NPCChatBoard 的人工点击与桥接调用同一个选择入口，保证 close 属性、默认 path、args 语义一致。服务器返回新对话、商店/INVOP 或输入面板时递增交互版本并完成对应观察后置条件。
- SM_STARTINPUT 建立有界 input_handle，桥接和 InputStringBoard 使用相同提交/关闭路径。用户填写导致 AI 暂停；文本不写审计日志，敏感输入值不通过资源回显。
- 开启、关闭、选择和输入分别返回操作；关闭属于 client_state，不能声称取消服务器已经开始的 NPC 脚本。未知选项效果超时为 unknown，旧选项拒绝且不发送网络消息。

### 拾取与使用物品

- pick_up 只处理当前角色实际所在格的已观察地面句柄，保持原 ActionPickUp/requestPickUp 动作链。站位不符时返回 need_move，而不是直接伪造 CM_PICKUP 坐标。
- 将 SM_PICKUPERROR、SM_UPDATEITEM、SM_REMOVEITEM、SM_GOLD 和地面列表变化转为正常更新后的证据。回报收到的物品/金币增量、部分失败和证据范围；单独“地面物品消失”不能证明是自己拾得。
- use_item 精确选择背包 item_id/seq_id/count，校验数量、已预留状态与 item 类型。药品/技能书走 requestConsumeItem；不支持物品类型立即返回 unsupported_item_type，技能书按当前单次消费语义约束 count。
- 库存变化仅证明已观察到消耗；生命增加或学会技能是另外的效果证据，不能承诺“扣了一瓶药就一定回满血”。没有相关回执/变化则 unknown，不再次发 consume。

### 战斗

- attack 通过 trackAttack/ActionAttack 和原有目标焦点执行。目标必须当前可见、存活、类型符合策略；PVP 只有 allow_pvp 且目标授权时允许。
- once 结束条件是该次攻击动作完成，scope 明确只代表动作；until_dead 使用原实时攻击节奏和正常追击，最长 30 秒。死亡事件/已知死亡状态可作为目标完成依据，离开视野或消失返回 target_lost，不报击杀。
- 将 checkMagicSpell 中技能类型分派抽成 typed 普通施法入口，键盘和桥接共同调用；输入为已学 magic_id 以及显式目标或格子，不伪造 SDL 键盘事件。
- 保留现有冷却、消耗、攻击动画和 requestMagicDamage 的时序；不单独暴露伤害请求。不支持的技能路径返回 unsupported_skill，支持列表由实现分支和角色已学技能共同生成。
- 开关技能显式报告 toggle 结果；新魔法动作不借用残留鼠标焦点。死亡、控制代次变化或目标无效时停止 AI 持续攻击，不清掉随后人工产生的焦点。

## 实施安排

1. 建立交互状态模型、NPC/输入句柄和统一面板入口，发布交互变化。
2. 加入拾取、使用物品适配及增量证据，区分 accepted、实际变化和 unknown。
3. 抽取普通施法入口、技能查询和有界攻击，接入所有停止原因。
4. 扩充假响应/真实适配路径测试，形成逐工具结果 fixture。
5. 向 T05 交付 NPC、INVOP、背包预留与事件观察接口；不在本任务实现交易报价逻辑。

## 检查与风险

- 构建 `client test_ai_bridge_actions test_ai_bridge_controller`；CTest 筛选 `^(ai_bridge_actions|ai_bridge_controller)/`。
- NPC：正确打开/选项/输入/关闭；过期选项、错误 NPC、越距、超长输入、面板替换、无新布局以及恶意指令文本仅作为数据。
- 拾取：完整/部分成功、被别人拾走、锁定、背包满、金币/物品混合、相同格重复 request_id；不能把 unrelated 库存更新强归因为本操作。
- 使用：药品和书、物品不存在、数量不足/越界、预留物品、其他类型、无回执、提交后接管及重复请求只发送一次。
- 战斗：普通攻击、已学技能/冷却、未学/不支持技能、目标死亡/失踪、角色死亡、超时、人工打断和边移动边施法；明确动作完成与伤害/击杀的区别。
- 禁用桥接回归键盘技能、鼠标攻击与 NPC 点击。实际场景由 T07 汇总；部分既有技能未实现不能靠新伤害报文“补全”。

## 交接依据

- 已分配MCP-T04-001，负责人A、游戏协作者B及主会话NPC分工见总计划；前置MCP-T03-001/r3/delivery1已接受。SpecFlow 入口 `C:\Users\leon\.codex\plugins\cache\specflow-local\specflow\0.2.1\skills\specflow\SKILL.md` 及同目录 `references/task-coordination.md`。
- 当前已授权 Implementation；后续授权/编号/基线由总计划提供，本文接收与交付节为写回位置，主会话读取审查，跨会话由用户转交摘要。

## 接收与进展

负责人A已接收MCP-T04-001，修订3；接收时间 2026-11-01 09:40:51（PowerShell Get-Date，Asia/Shanghai）。执行者01a19251-a370-7491-b33e-f7afef6be379，基线HEAD b1ddda8加已接受T03/r3/delivery1和当前已登记并行修改。已读意图、总计划、T04、T03交付/API、只读准备及SpecFlow协作契约。独占纯actions helper、actions测试、CMake、本文和最终manifest，统一共享build；B及主会话独占游戏/NPC范围。无Git写入、不新增agent、不改用户DB/下载包、不自行T05。主会话NPC部分按同一分配执行，接收与进展写.runtime/collaboration/T04-npc-report.md，不并发修改负责人主记录。

| 时间 | 分配编号 / 执行者 | 接收、进展或阻塞 | 证据 / 回报位置 |
| --- | --- | --- | --- |
| 2026-10-31 | 无 / 无 | 仅完成局部计划 | 本文 |

## 交付与审查

交付时记录：MCP-T04-001/r3/delivery1完成后等待主会话审查，由主会话接受后再分配T05；此后两项均已局部接受。下文最终交付节及manifest仍为T04精确版本依据，此前进展/失败快照保留历史，不覆盖最终版本。

### 2026-11-01 09:46:59 接口与开发检查

- actions.hpp/.cpp已落盘：六类动作共享typed观察、真实发送sticky、动画开始/结束、同身份死亡、精确物品实例下降与不相关奖励分离；终态冻结。NPC纯模型由主会话实现，与actions helper互不依赖。接口消息T04-a-api.txt已向B/主会话核对UUID并排队；queued不代表对方接受。
- 已注册actions/npcmodel纯库及npc/npc_gui测试，真实adapter保持client_objs。所有共享build由A执行。当前统一client/server与相关测试构建进行中，尚未交付。
- t04-pure-build.log首次19/19编译成功；t04-pure-ctest.log首轮59/60通过（NPC8/8）。唯一新用例失败为fixture缺物品策略授权及错误假定所有动作超时自动暂停；已修fixture并显式manualInput后验证迟到证据不恢复，待复验。历史失败日志保留，不宣称全过。
- 首次接收时T03的10产物/15证据散列全一致，产品14一致/9为已登记B并行T04修改；不修改T03 manifest。主会话NPC布局/InputLine/XMLTypeset必要窄依赖已主动登记，属预期协作。
- 源码审查要求B阻止AI旧攻击/pickup trigger自动重发，并修queued意图kind，保留人工续发；主审PVP范围/辅助技能/拾取证据要求继续落实。真实CM_PICKUP缺严格相关回执，不能用fake correlatedReceipt成功路径冒充真人结果。
### 2026-11-01 10:00:38 开发检查与审查收尾

- 第四轮build 76/76 client与NPC GUI链接成功；115/115相关CTest通过，27.89秒，其中actions28/controller26/runtime10/protocol13/security11/callback9/netio3/widget2/NPC9/NPC GUI4。历史日志已另存t04-build-first-pass.log和t04-regression-first-pass.log，不能覆盖其后PVP与typed目标参数收紧。
- 三次编译诊断依次为combatadapter skillboard头路径、NPC GUI测试EmojiDB前置声明、NPC所需InventoryBoard::aiInvOp getter；均由各自独占执行者修复。A不代改游戏/NPC文件。
- 真实有害攻击使用所有当前可见其他玩家的保守超集，纯validateCombatPeers逐个授权，发送guard和续攻重检；辅助技能不得误当PVP。A已追加validateCastTarget共享参数语义：实体技能不能把x/y误当实体目标或默默转自身，真实有益技能支持显式自身/缺省自身。等B最后接线冻结，再完成最终增量、散列和交付。
- 主会话T04-review-return明确接受现有普通拾取缺相关成功回执的限制：保持unknown，failedItemID只作为未关联服务端证据，未知seq_id不得用0伪造failed_items。partial/物品或金币增量不作完整归因；不改变schema或服务端。
- NPC pure9/gui4已过，GUI为真实client_objs+字体+dummy SDL；并非真实登录角色操作。T04仍在本分配开发检查，不开始T05。
## 2026-11-01 10:06:40 最终开发交付：MCP-T04-001/r3/delivery1

### 输入与精确版本

- 分配MCP-T04-001，方案修订3；负责人A 01a19251-a370-7491-b33e-f7afef6be379。输入为已接受MCP-T03-001/r3/delivery1及其manifest，HEAD b1ddda8d8742afb1d60839f759733a2e8ee26207；未执行Git写操作。
- 最终manifest：.runtime/collaboration/T04-delivery-manifest.json，33产品文件、12构建产物、23证据；记录T03输入manifest SHA256和每项最终SHA256。B13与主会话NPC15产品对其source-hashes零差异，A5文件为actions.hpp/.cpp、actions测试及两处CMake。
- 游戏协作交付快照T04-game-report-snapshot.md、NPC协作交付快照T04-npc-report-snapshot.md；API交接T04-api-handoff.md。全部在.runtime/collaboration，均纳入manifest，活跃报告后续纯消息补记不改变快照证据。
- 同配置签名VENGINEERING-20261101094505；client SHA256 5A8C84DECC58A2E7A29616C3F7D242904C131101A860C5AB488E7B863339BC4C；server SHA256 AD910534ED2085529EC09BA0FB6C0E7B68FCB78B1A85A57EFDC4A8249A54E6A2。Client最终10:02:41链接晚于游戏适配10:01:28冻结源码。

### 实际变更与行为

- 新增10个真实工具：get_skills/get_interaction、npc_open/choose/input/close、pick_up/attack/cast/use_item；真实C++能力从9变19。不注册T05经济stub，Python35固定工具不等于客户端35能力。
- ActionProgress共用六类结果判定：真实发送sticky、旧context/死亡/期限优先、终态固定、未发取消与已发unknown、真实动画完成client_state、同身份死亡observed_postcondition、精确实例消费数量下降。所有result/evidence复用v1封闭schema；迟到由原controller记录late_resolution，不重放。
- 拾取仅当前格原ActionPickUp→动画→requestPickUp；拾取trigger再次核context/deadline/地面revision。只列原地面物品类型的实际库存变化，金币仅原地面含金币时列观察；失败ID来自实际SM_PICKUPERROR。无相关回执的结果保持unknown，不把地面消失或无关奖励算自身拾取。
- 使用药品/书走一次requestConsumeItem，精确item_id/seq_id/count、已知库存、暂存/预留同身份busy；书count=1。数量下降只证明观察到消费条件，不保证治疗/技能效果。其他物品类型unsupported_item_type。
- 普通攻击保留trackAttack/ActionAttack/服务器伤害与动画时序，AI自动续打由adapter有限调度；人工续打不变。追击用4096轮原路径搜索及单位hop，AI队列不进入原无限寻路。once只表示动作完成，until_dead必须真实同身份死亡；丢失目标target_lost，校正/死亡/跨图/接管停止旧意图。
- typed cast与原键盘共用ProcessRun::castMagic；支持项由真实服务器分支和已学技能生成，未知列表标unavailable_fields。validateCastTarget拒绝实体技能仅坐标误施、允许有益技能明确自身、Ground/Self/Toggle按真实入口验证；不继承旧鼠标焦点。支持24种分类路径（10 target/4 ground/5 self/5 toggle），其他已学技能supported=false；详见B报告。
- validateCombatPeers用于实际validate、tick与发送guard：有害路径保守检查全部当前可见其他玩家，包含显式怪物目标范围技能、半月/十方等开关攻击；独立PVP和每个peer授权缺一不可。可能拒绝实际伤害范围外但仍在视野的未授权玩家。自身有益目标不套其他玩家peer限制，其他辅助目标仍需peer授权。
- NPC选择只来自真实渲染叶与同一GUI入口，保留path/id/args/close/single语义；来源绑定instance/session/world、真实serverPanel与GUI revision，generic输入或旧GUI不得生成新场景权限。InputStringBoard共用校验/提交，199 UTF8字节实际容量，文本不进入快照/审计，先清旧回调再调用以支持重入。关闭仅client_state；新同NPC面板为观察后置条件，不保证脚本其他效果。
- runtime整合combat/npc并隔离SM通知异常，仍只关闭桥接而保留普通游戏；无预算deadline、disconnect优先、人工接管、T03分页/句柄/主线程有界调度均保留。InventoryBoard仅新增aiInvOp只读getter，不实现T05交易。

### 实际检查与修复

构建由A独占，入口build-t04-all.sh/rebuild-t04-all.sh；最终命令等效为：

~~~text
cmake --build build --target client server test_ai_bridge_actions test_ai_bridge_npc test_ai_bridge_npc_gui test_ai_bridge_controller test_ai_bridge_runtime test_ai_bridge_protocol test_ai_bridge_security test_response_callbacks test_netio test_widget --parallel 8
ctest --test-dir build -R '^(ai_bridge_actions|ai_bridge_npc|ai_bridge_npc_gui|ai_bridge_controller|ai_bridge_runtime|ai_bridge_protocol|ai_bridge_security|response_callbacks|netio|widget)/' --output-on-failure
~~~

- t04-build.log最终12/12完成；t04-regression-ctest.log最终116/116通过、27.72秒。分组actions29/controller26/runtime10/protocol13/security11/response_callbacks9/netio3/widget2/NPC9/NPC GUI4。
- 实际共享helper覆盖六类成功语义/拒绝/unknown/失效/死亡/期限/接管、精确消费与无关奖励、攻击动画和目标丢失、多玩家授权、实体/地面/自身施法参数；配合真实controller测试request_id去重零重发、入队人工接管零执行、timeout后late_resolution保持unknown及不恢复权限。
- NPC纯控制器测试覆盖入队换布局、已发脚本去重、无新布局unknown与迟到不重发；4项真实client_objs+字体+dummy SDL GUI测试覆盖事件叶/single/布局重入、输入原校验/空白处理/回调重入、generic替换使旧NPC输入失效。
- 首轮59/60失败是新增测试fixture策略缺授权与错误假设所有动作超时自动pause，已明确修fixture并使用manualInput验证晚到不恢复；编译先后修复skillboard头路径、EmojiDB声明、aiInvOp getter。历史失败/115项通过均单独保留，最终116项覆盖最后修复。
- t04-up-to-date.log：ninja no work；t04-whitespace.log：33产品行尾空白0、git diff --check退出0。曾将no-index对空文件的退出1误判为空白失败，已按真实输出与逐文件检查纠正，未据此改产品。

### 最终binary真实只读互通

主会话独立执行T04-client-smoke-report.md，与上述最终client hash一致。官方SDK→Python stdio→真实C++私有凭据认证通过；Python35工具/6资源，C++恰19真实能力。未RUN时state/nearby/inventory/skills/interaction均not_in_game；events与stop通过，随后paused。PID85844正常CloseMainWindow五秒退出，凭据1→0；未强杀、不声称无法取得的ExitCode为0，没有启动server/创建DB或修改存档。该报告及4个原始证据纳入manifest。

### 限制、偏差及后续输入

- 真实CM_PICKUP无成功关联回执，因此实际拾取不能确认succeeded。主会话T04-review-return已明确允许如实unknown；不虚构seq=0的failed_items，不伪造partial成功。纯correlatedReceipt成功测试不是当前真实协议的成功证据。主会话接受的是当前诚实能力和限制，不能宣称拾取真实效果已验收。
- NPC GUI组件测试和未RUN只读互通不能替代真人角色NPC/拾取/攻击/施法/消费/人工接管/死亡/跨图/断线场景。native GUI pipe unavailable持续；真实角色验证留T07汇总。未跑POSIX、未量测完整主线程2ms软预算分布。
- 已提交游戏效果不能保证撤回；client_state动画/开关不表示命中、伤害或服务器确认。未支持技能/物品明确拒绝，不通过新的伤害协议补齐。无schema/Python契约改动、无额外agent、无用户DB/下载包或版本检查修改。
- T05可使用NPC resolvePanel/current shop/invop句柄、原T03物品/目标身份及冻结分页、实际预留观察和当前控制器未知经济锁；必须按真实事务证据接入，不把unknown当完成。仅主会话可接受本交付并另行分配T05。

本交付请求主会话审查并重新分配。负责人保持当前产品源码冻结，不自行开始下游。
