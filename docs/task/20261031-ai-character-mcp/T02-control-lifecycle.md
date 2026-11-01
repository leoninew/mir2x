# 子任务 T02：主循环、操作生命周期与人工控制权
最后修改时间: 2026-11-01 19:26:21

## 审查状态

局部方案 Accepted；`MCP-T02-001/r3/delivery1` 已于 2026-11-01 08:49:41 由主会话 accepted。必要的 Hero/CreatureMovable 续发中断窄入口属于已登记运行时范围。

- [x] 控制器、runtime、面板、生命周期与 74 项交付回归，以及当时最终二进制只读 stdio 冒烟已接受。
- [ ] 最终版本真实 RUN 面板、人工接管和动画/生命周期联动仍待验证，见[验证清单](../../verification/20261031-ai-character-mcp.md)。

下文是 T02 交付时证据。08:55:41 的 deadline 顺序调整已纳入后续 T03，不回写 T02 manifest；当前整体版本见总计划，本次不重新打开任务或运行检查。

## 父任务与权威记录

- [总意图](../../intent/20261031-ai-character-mcp.md)、[权威总计划](../../plan/20261031-ai-character-mcp.md)。任务 T02，协调根 `D:\SourceCodes\opensource\mir2x`。
- 依赖、分配和交付接受以总计划为准。

## 目标与验收

将 T01 桥接接入真实客户端，建立上下文、人工优先、操作账本和安全回调。覆盖 A01、A03、A04、A05、A06。即使动作尚由 fake 提供，也能证明接管后旧动作无法再执行、退出不会访问失效对象。

## 范围与文件

- `client.cpp/.hpp`、`main.cpp`、`processrun.cpp/.hpp`、`myhero.cpp/.hpp` 的生命周期和中断入口。
- 新增 `client/src/aibridge/controller.*`、`operation.*`、`events.*`，以及遵循现有 GUI 组件习惯的 AI 控制面板。
- 新增 `client/test/unit/test_ai_bridge_controller.cpp`；修改测试 CMake。
- 不实现具体 NPC/经济工具；控制器提供按固定方法注册动作适配器的内部接口，不暴露任意外部方法注册。

## 输入

直接前置 T01 与 T08，须由主会话分别接受 schema/认证传输/策略和回调生命周期修复及测试。实际接收时记录可用版本，使用其 DTO 和固定 v1 错误语义，不复制第二套协议类型。

## 局部技术设计

1. `AiBridgeController` 持有实例/会话/地图/控制代次、不可变策略、操作账本与事件环；游戏适配接口负责 snapshot、validate、begin、tick、cancel。接口返回结构化证据，不能把 bool 作为动作完成值。
2. 主循环中的游戏网络和 SDL 事件先于桥接动作分派；Process 切换、断线、死亡、地图变化主动通知控制器并暂停。单独失焦/聚焦仅更新观察状态，不切换控制权，确保用户能切到 AI 宿主下指令。游戏状态不可用时允许 get_session/get_operation/get_events/stop，其他方法返回 not_in_game。
3. `Client::processEvent` 在原 GUI 分派前检测冲突输入。先递增控制代次、作废未执行请求、取消 AI 焦点/动作，再传递原事件。只暂停 AI 所有的持续操作，不在 read_only 下干预正常玩家队列。
4. 新面板显示“未启用/只读/AI 控制中/已暂停”、角色身份和原因，提供开启/恢复与暂停按钮。开启只能绑定当前已登录角色，禁止提供同等 IPC 方法；按钮事件从冲突检测中准确排除，不普遍豁免所有 UI 点击。
5. 账本记录 request_id、规范化输入、operation_id、上下文、开始/截止时间、提交标记、证据与终态。最多一个改变状态的操作；同一连接账本不驱逐，重复请求只查询，不再次执行。
6. stop 在每次处理其他动作前优先消费，可在 paused、旧 control_epoch 或账本满时生效。按提交标记选择 cancelled 或 unknown；已发送操作不伪造“撤销成功”。map/session 变化与断开同样使等待请求失效。
7. 补 Client scoped send：单调 response ID、不重置、取出回调后调用；Process generation 绑定；主线程到期/取消通知一次；未知迟到响应丢弃并计诊断。桥接回调使用弱生命周期 token 和 operation_id，不捕获跨生命周期裸 this。
8. 退出采用显式 shutdown 和主循环范围守卫；不能依赖当前未调用的全局 Client 析构。停止接收、暂停操作、关闭连接、清理本实例文件，所有步骤幂等且不等待远端。
9. 审计写 JSONL，包含方法名、operation_id、上下文、状态、证据类型和错误码，不记录 token/NPC 输入文本。每文件 5 MiB、保留 3 份，均在本实例运行目录；持久日志不能作为重启后自动重放队列。

## 实施安排

1. 实现纯控制器与 fake 游戏适配器，完成状态、超时、去重、预算预留基础和事件环。
2. 接收 T08 的 response ID/分派/超时/作用域取消接口，将桥接和 operation_id 接到 scoped send；保留 T08 重入测试。
3. 将 transport/controller 接入主循环和显式清理，接入会话与地图状态变化。
4. 增加可见控制面板和输入优先规则，提供停止游戏动作的窄接口。
5. 向待派发的 T03 输出注册/观察 API、会话样例、接管与 unknown fixture；向已独立接受的 T06 提供集成说明。T02 审查接受后的下游放行为 T03，不再将 T06 列为待放行任务。

## 检查与风险

- 构建 `client test_ai_bridge_controller test_netio test_widget`；CTest 筛选 `^(ai_bridge_controller|netio|widget)/`。
- 用同一轮事件模拟“AI 请求已收但未执行 → 人工点击 → 旧请求”，断言没有 AI 发送且原点击仍执行；覆盖 GUI 消费输入、键盘、滚轮、文本、鼠标移动、恢复按钮，以及仅切换到宿主窗口不暂停的行为。
- 覆盖未登录、RUN 切换、同角色换图、换角色、死亡、桥接/游戏分别断开、关闭窗口和异常清理。
- 覆盖重复参数冲突、账本满时 stop、慢读导致 disconnect、未提交取消、已提交 unknown、超时与回执同时到达。
- 回调测试包含表为空后再次发送、迟到旧 ID、回调中再次注册、Process 被销毁、重复回执和超时只触发一次；不得用忽略异常替代去除失效指针。
- 主线程预算在假负载和后续真实运行中测量；它是有界轮询目标，不承诺硬实时 2 ms。

## 交接依据

- 已分配 MCP-T02-001；负责人/协作者和独占文件以总计划“T02 有效执行分工”为准。协议入口为 `C:\Users\leon\.codex\plugins\cache\specflow-local\specflow\0.2.1\skills\specflow\SKILL.md` 和同目录 `references/task-coordination.md`。
- 当前已授权 Implementation，实际授权/基线/前置版本由总计划派发记录给出；可写回报位置为本文接收与交付节，主会话读取审查。
- 跨会话输出可复制摘要，由用户转交；不以计划路径自行取得执行权。

## 接收与进展

已接收 MCP-T02-001，负责人 01a19251-a370-7491-b33e-f7afef6be379，接收时间 2026-11-01 07:47:54。已核对方案修订 2 与有效分工；独占 controller/operation/events/审计日志辅助、控制器测试、CMake 和本记录。运行时协作者独占 runtime/GUI/Client 钩子，负责人统一构建。前置 T01 的24文件manifest已核对无差异、24/24测试和完整client链接通过；T08的9/9 CTest、54断言及client.cpp编译检查通过，已由主会话接受。

| 时间 | 分配编号 / 执行者 | 接收、进展或阻塞 | 证据 / 回报位置 |
| --- | --- | --- | --- |
| 2026-10-31 | 无 / 无 | 局部方案完成，等待方案审查 | 本文 |

### 实施进展

- 已与运行时协作者确认自包含 controller.hpp/operation.hpp：查询 snapshot、validate/begin/tick/cancel、实际 stop hook、submitted/证据/结果/late_resolution 回传和弱生命周期标记。
- 控制器、账本、预算基础、事件环、审计日志及 fake/可控时钟测试首版完成；不新增 v1 字段，不注册 stub 业务能力。
- 首轮定向编译成功，控制器 15/15 CTest 通过（5.15 秒），日志 .runtime/collaboration/t02-controller-ctest.log。运行时仍在协作者范围实施，尚未整体 client 验证。
- 2026-11-01 08:21:01：方案修订3已同步，T06改为直接依赖T01，不阻塞T02。控制器23/23 CTest通过（5.16秒）；新增执行前校验跨截止/重入接管用例待下一轮。统一client编译首轮发现aiState为private，已交运行时协作者修正；新增runtime测试已注册，下一轮统一回归。

- 2026-11-01 08:34:57：人工服务器校正续发修正已随client重新链接，72/72通过（25.69秒）。收尾复核发现runtime在队列满的busy响应分支未计processed，已交协作者修正并请求冻结；待该项增量验证后提交manifest。主会话另在独立client副本做只读冒烟，结果按其具体版本记录。

## 交付与审查

交付时间：2026-11-01 08:45:15（PowerShell Get-Date，Asia/Shanghai）。分配 **MCP-T02-001**，方案修订 **3**，本次交付版本 **MCP-T02-001/r3/delivery1**。实现与最后编译/回归已完成，已纳入所有收尾修正，提交主会话审查；不自行标记实现 accepted，不启动下游。请审查并重新分配。

### 实际文件与行为

共27个产品/构建/测试文件，加本任务记录共28文件；精确路径及SHA-256见 `.runtime/collaboration/T02-delivery-manifest.json`，路径清单见 `T02-files.txt`。

| 范围 | 实际文件 | 交付内容 |
| --- | --- | --- |
| 纯控制器 | `client/src/aibridge/controller.hpp/.cpp`、`operation.hpp/.cpp` | 四态控制、当前角色session与world/control代次、4096连接内不可驱逐去重、立即accepted/operation_id、单写动作、双重校验、提交标志和六态结果、deadline/late_resolution、未知经济锁与预算预留 |
| 事件与审计 | `client/src/aibridge/events.hpp/.cpp`、`audit.hpp/.cpp` | 1024条事件/gap/游标；5MiB当前+2份轮转日志；仅方法/operation/context/状态/证据类型/错误码，不存params/token/NPC输入；私有权限、重解析/链接检查和审计故障失败关闭 |
| 真实运行时 | `client/src/aibridge/runtime.hpp/.cpp`、`runtime.intent.hpp` | transport/controller持有、断开优先、stop优先、有界队列、1ms transport/2ms总体软预算、真实SDL输入门控和控制面板、T08弱生命周期回执、AI意图代次 |
| 生命周期与游戏入口 | `client/src/client.cpp/.hpp`、`main.cpp`、`processrun.cpp/.hpp`、`processrunnet.cpp`、`myhero.cpp/.hpp`、`hero.cpp/.hpp`、`processlogin.cpp`、`gui/controlboard/cbleft.cpp` | 网络/人工输入先于动画及桥接；RUN/地图/死亡/断线失效；仅本窗口完整按钮手势恢复；AI旧续发停止且保留人工路径；显式shutdown/范围守卫/atexit及退出调用点 |
| 构建与测试 | `client/src/CMakeLists.txt`、`client/test/unit/CMakeLists.txt`、`test_ai_bridge_controller.cpp`、`test_ai_bridge_runtime.cpp` | 纯库与client runtime分离、26控制器fake/可控时钟用例、10共用输入/意图门控用例 |

运行时16文件由协作者01a19206-195c-7ce2-a9f8-866c17dc7d18交付，已读取并纳入 `.runtime/collaboration/T02-runtime-report.md`；负责人未越权修改。无需改CreatureMovable即可在MyHero先使旧续发stamp失效。T06独立Python实施已按修订3与T02解耦，并已由主会话审查接受（76/76 Python测试）。T02不重做或重新放行T06；本次交付审查后的下游分配为T03，由主会话决定。

### 契约与下游接入

- 未修改v1 schema/fixtures、T01 transport/credentials/policy或T08 ResponseCallbacks。T01的24文件manifest复核：22文件散列完全一致，仅两处授权CMake增加T02入口；T08响应管理头文件和独立测试保持接受版本，Client在此基础接运行时。
- 当前真实客户端仅宣告 `game_get_session`、`game_get_operation`、`game_get_events`、`game_stop` 四项内建能力。无T03/T04/T05 stub；未来真实适配器注册后才加入capabilities。
- `RuntimeState`负责快照；Query返回固定data；ActionAdapter提供validate/begin/tick/cancel，OperationUpdate显式outcome/submitted/completionScope/evidence/error/result。begin只由advance触发，执行前再查上下文、策略、预算和截止；成功不能以bool或仅发送表示。
- 新增notifySnapshot()发布固定snapshot事件/当前context/空operation_id，域观察器负责每帧合并最多一次；未进RUN不发布。新增checkDeadlines()只检查并取消到期工作，运行时每轮请求前后无条件执行，不被2ms普通工作预算跳过，绝不begin/tick。
- 游戏入口经claimIntent/setIntentFocus/releaseIntent和runtime.sendScoped。回执decoder只解析，不捕获跨Process裸指针或续发动作；旧operation只追加late_resolution，不能恢复授权。
- IPC重连/人工恢复/临时loading不重置同登录的经济预算和unknown锁；明确登出/游戏断线后新RUN或换角色才重置。登出保留进程instance_id、transport/controller及冻结policy，不自动换实例。
- 更详细接口约定：`.runtime/collaboration/T02-api-handoff.md`。会话和六态样例继续使用已接受T01契约fixture；本轮所有控制器响应、operation和事件亦按该schema验证。

### 本轮检查与修复

统一构建者为负责人，环境参考仓库 `.vcpkg/build-msys2.sh`，MSYS2 UCRT64/GCC、固定vcpkg baseline和现有依赖；未全量安装/打包资源，未改下载制品或全局配置。执行入口 `.runtime/collaboration/build-t02-all.sh`：

```text
cmake -S . -B build
cmake --build build --target client test_ai_bridge_controller test_ai_bridge_runtime test_ai_bridge_protocol test_ai_bridge_security test_response_callbacks test_netio test_widget --parallel 8
ctest --test-dir build -R '^(ai_bridge_controller|ai_bridge_runtime|ai_bridge_protocol|ai_bridge_security|response_callbacks|netio|widget)/' --output-on-failure
```

最终client.exe链接成功，CTest **74/74通过，25.86秒**：controller26、runtime10、T01协议13/安全11、T08响应9、netio3、widget2。证据 `.runtime/collaboration/t02-configure.log`、`t02-build.log`、`t02-regression-ctest.log`、`t02-lasttest.log`。最后增量入口为rebuild-t02-all.sh；check-t02-up-to-date.sh返回ninja: no work to do（t02-up-to-date.log），确认最后源码与产物一致。Git diff空白检查及新增控制器源的no-index检查通过，不将其当编译证据。

覆盖未登录/角色/地图/RUN/死亡、人工接管后不发送、busy、规范化去重冲突、4096满账本仍stop、未提交cancelled/已提交unknown、超时回执竞争及重入、二次validate跨截止或重入暂停、事件gap与域snapshot通知、普通工作零预算下的超时取消、重连旧连接隔离、经济unknown持额、loading不重置预算、审计脱敏轮转/故障关闭、精确按钮与原人工输入、旧续发stamp不复活及后续人工stamp有效。

过程修复：预算耗尽仍执行checkDeadlines、busy响应纳入8请求计数、snapshot域通知入口、内部controller故障关闭桥接、人工服务器校正续发兼容；aiState公开只读入口；runtime busy分支括号；按钮命中采用与Widget一致的半开区域并测试右/下边界；普通登出不重建实例；主SDL窗口过滤；回执异常只关闭桥接；自己的OFFLINE切离RUN。aiState访问级别和括号错误由编译诊断发现，按钮边界由首轮71/72回归发现，均已修复并在最终74/74复测。

假负载诊断（4096次request+advance，含测试/schema开销）：均值1019.07us、最大7401.5us，原始输出在t02-lasttest.log。该测量不含transport、磁盘审计或游戏渲染，不能当真实帧预算保证；软预算允许单次不可抢占工作超限。

### 未运行、风险与版本边界

- 主会话独立旧版本开发冒烟：`.runtime/collaboration/T02-client-smoke-report.md`，08:28:45副本/build VENGINEERING-20261101082828（SHA256 92fe6ff9df36c433dfaabcbad3b3e5d214137a34c13f1bc8352b181bddc9f2c0）。真实官方SDK stdio→Python→C++凭据ACL/认证/session/events/stop通过，服务器未启动、phase=disconnected；WM_CLOSE五秒内退出，凭据1→0，无强杀。该结果不冒充最后补修binary的运行验证。
- 未做真实GUI/DPI/模态窗口点击、实际Hero动画续发中断、各退出路径的进程级演示或Python→游戏端到端操作；这些仍属T07集成证据。已交付真实运行时代码，纯门控测试不冒充真实场景验证。
- POSIX分支未在Linux/macOS构建运行。本次真实构建/安全回归环境为Windows UCRT64。
- 2ms是软预算，schema/停止/审计I/O/面板刷新不可抢占；真实游戏耗时分布未测。未知经济结果保守锁定直到新登录会话，不自动补偿/重放。
- 既有Hero法术视觉addDelay的裸this/静态mapID模式未在本任务扩范围重写；未来域适配不可复制该模式。
- 基线HEAD仍为 `b1ddda8d8742afb1d60839f759733a2e8ee26207`。代码及client/测试二进制散列、日志散列记录于交付manifest；未暂存/提交Git，未改用户存档，未新增agent。



- 2026-11-01 08:45:15：最后74/74回归及client链接完成；notifySnapshot、checkDeadlines、busy计数、内部故障关闭均包含在此轮。Client SHA256 A14E9FB9E3F1F3570391D61CF0D4AC6E52DFFAE7D43722997B8AFCFD7ABFE041。请主会话审查并重新分配。
