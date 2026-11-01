# 子任务 T03：当前角色观察与移动闭环
最后修改时间: 2026-11-01 19:26:21

## 审查状态

方案 Accepted；`MCP-T03-001/r3/delivery1` 已于 2026-11-01 09:32:50 由主会话 accepted，沿用总计划修订 3。

- [x] 观察/移动实际接入、93 项交付回归及当时最终二进制 9 能力只读 stdio 冒烟已接受。
- [ ] 真实角色观察、移动/停止/接管与地图生命周期仍待验证，见[验证清单](../../verification/20261031-ai-character-mcp.md)。

下文“等待审查/未启动 T04”等是交付时记录；T04、T05 此后已分别接受。当前执行状态以总计划为准，本次仅同步文档，保留历史 manifest 和检查证据。

## 父任务与权威记录

- [总意图](../../intent/20261031-ai-character-mcp.md)、[权威总计划](../../plan/20261031-ai-character-mcp.md)。任务 T03，协调根 `D:\SourceCodes\opensource\mir2x`。

## 目标与验收

读取当前角色和客户端已知世界，以普通动作完成当前地图内移动与停止，覆盖 A01、A02、A03、A04、A05。快照与工具操作指向同一角色/地图，未知信息不伪造成精确值。

## 范围与文件

- 新增 `client/src/aibridge/snapshot.*`、移动及查询适配实现。
- `processrun.cpp/.hpp`、`processrunnet.cpp`、`myhero.cpp/.hpp` 中必要只读入口与状态更新通知。
- `client/test/unit/test_ai_bridge_actions.cpp` 和测试注册；此文件由 T03 起建，T04 顺序追加。
- 不增加跨图导航、截图、自动跟随、传送或后台读取游戏内存。

## 输入

直接前置 T02 已接受，包含操作注册接口、上下文/代次、stop、事件发布与 fake 适配器。依赖 schema 来自其 T01 基线。接收时记录实际版本。

## 局部技术设计

1. 快照只在主线程生成，先检查 RUN 和 hero 存在，再调用可能抛异常的 getMyHero。读取 mapUID、位置/动作终点、朝向、生命魔法、等级、金币、已知状态、背包/腰带/装备；缺失名称/血量等保持 null。
2. nearby 来源 getCOList、客户端实际视野及地面物品列表。target_handle 包含生成时的会话、地图代次及对象身份；重新使用前查找 UID 并校验目标仍在有效观察范围。地面 handle 不伪造服务器未提供的物品唯一 ID。
3. 地图区域是当前加载地形的有限矩形；边界之外返回 out_of_bounds，动态阻挡另列来源和时刻，不把静态可行走格子视为保证可达。
4. 序列化前建立本次不可变 DTO，分页复用该快照并设置有限缓存；每类保留最近两个快照，游标过期明确失败，不能一页读取旧背包、一页读取新背包。
5. move_to 校验当前 context、角色存活、整数地图坐标和 tolerance 0..2，无活动交易/模态阻塞；由 currMotion 的 endX/endY 构造现有 ActionMove，不用插值中的屏幕位置作为出发点。
6. 继续使用客户端寻路和服务端正常 CM_ACTION。到达容差范围且动作停止作为 observed_postcondition，包含实际位置和最新服务端位置证据（如有），不伪称服务器相关回执。路径构造明确失败可报 unreachable；3 秒无进展报 stalled；超时/取消停止后续 AI 移动。
7. 不自动替换前一个动作：busy 时拒绝。换图/掉线/手动移动后旧目标失效；如果目标是传送格，地图变化停止原操作，不追到下一地图。

## 实施安排

1. 实现查询 DTO、对象句柄、有限分页和地图区域，接入状态更新后的 snapshot/event 发布。
2. 实现 move_to 和观察后置条件，连接控制器停止/超时接口。
3. 用假地图/假对象验证算法边界，用 client_objs 适配测试确认调用的是现有移动路径。
4. 记录给 T04/T05 的对象、背包、地面句柄和可用游戏适配接口；与 T06 fixture 校验保持一致。

## 检查与风险

- 构建 `client test_ai_bridge_actions test_ai_bridge_controller`；CTest 筛选 `^(ai_bridge_actions|ai_bridge_controller)/`。
- 查询覆盖无角色、缺失名称/血量、64 位 UID、大金币、对象离开视野、对象复用、分页中状态变化、越界区域和未知字段。
- 移动覆盖已有部分动画、当前格、可达路径、静态阻挡、动态阻挡、地图边缘、超时、人工接管、服务端纠正位置以及换图。
- 核对没有 CM_REQUESTSPACEMOVE、调试传送或修改坐标；禁用桥接时原有点击移动不变。
- 真实窗口读取/移动/停止由 T07 汇总验证；此任务可提前做开发冒烟并回填证据，但不能将 fake 位置测试记成实际游戏完成。

## 交接依据

- 已分配 MCP-T03-001；负责人01a19251-a370-7491-b33e-f7afef6be379、游戏适配协作者01a19206-195c-7ce2-a9f8-866c17dc7d18，独占范围见总计划。SpecFlow 入口 `C:\Users\leon\.codex\plugins\cache\specflow-local\specflow\0.2.1\skills\specflow\SKILL.md` 及其 `references/task-coordination.md`。
- 当前已授权 Implementation；工作区为协调根，执行基线/编号/授权来源由总计划给出。本文接收与交付节为回报位置，主会话读取；跨会话由用户转交可复制摘要。

## 接收与进展

已接收 MCP-T03-001，负责人01a19251-a370-7491-b33e-f7afef6be379，接收时间2026-11-01 08:55:56（PowerShell Get-Date，Asia/Shanghai），方案修订3。已核对总计划有效分配/独占范围、HEAD b1ddda8d8742afb1d60839f759733a2e8ee26207及T02前置MCP-T02-001/r3/delivery1；manifest的28文件/8产物/8证据共44散列全部一致。负责人独占纯snapshot/句柄/分页/移动helper、actions测试、CMake和本记录；协作者独占真实游戏适配与runtime注册。共享build仅负责人执行，最后重配置后统一构建client/server；不动用户存档/下载制品/版本检查，不新增agent或Git写入。正在确认可fake的自包含接口。

| 时间 | 分配编号 / 执行者 | 接收、进展或阻塞 | 证据 / 回报位置 |
| --- | --- | --- | --- |
| 2026-10-31 | 无 / 无 | 仅完成局部计划 | 本文 |

## 交付与审查

交付时记录：实施及开发检查完成，版本MCP-T03-001/r3/delivery1，等待主会话审查、不自行启动T04。此后主会话已接受并继续T04/T05；本节精确证据及manifest仍对应当时T03版本。

## 2026-11-01 09:02:55 接口冻结与首轮检查

- 负责人已通过codex-cli确认B的gameplay.*、ClientCreature创建代次getter及真实SM已知标记范围；主会话已读纯接口并认可方向。冻结snapshot.hpp/movement.hpp公开签名：CharacterSnapshot/ObservationHandles/FrozenPages/MoveProgress，纯代码不引用Client/SDL/游戏对象。
- 句柄只绑定instance/session/world及真实对象incarnation/地面revision；control_epoch变化不作废只读快照。当前可见集合每次更新使离场身份失效，冻结旧页不能重新激活它。分页每类最近2份，每页100行，总行数上限8192；附近地面单行物品超过契约100项受控unavailable，不截断。
- 地图范围先用64位计算校验再访问；静态walkable来自地图，dynamic_blocked只能标示真实已知视野状态，未知null。附近半径为Chebyshev格距离，输入须先经真实视野过滤。
- MoveProgress使用实际格位置/停止状态和客户端证据，3秒无格进展stalled；30秒截止优先于成功，提交状态保持sticky。始终由运行时checkDeadlines处理安全超时，普通工作预算不能跳过。
- 首轮纯构建及CTest实际通过38/38（actions12 + controller26，5.13秒），日志t03-pure-build.log/t03-pure-ctest.log。随后新增sticky提交和超大地面查询受控错误两项回归，待增量验证；不把此前38/38描述为新源码验证。
- 尚未编译本轮真实游戏适配/client/server，B正在独占实现；真实GUI环境受主会话报告的native pipe不可用影响，未声称窗口角色移动通过。

## 2026-11-01 09:14:44 真实适配首轮构建与主审收尾

- B实际文件名为gameadapter.hpp/.cpp（不是此前拟议gameplay）；runtime已接五项新能力，client/server同配置Build signature VENGINEERING-20261101090730完成140/140链接，首轮相关回归89/89通过，26.32秒。历史证据已另存t03-build-round1.log/t03-regression-round1.log，B编译期间及之后的修正待增量复建，不用本轮证据替代最终源码验证。
- 新actions用例实际复用客户端AStarPathFinder与fake地形，覆盖可达、地图边缘、墙和动态阻挡。首轮该用例因测试复用带缓存finder失败；改为与CreatureMovable一致的每次新建solver，保留不可达断言，完整89/89含修复后通过。没有修改common寻路算法。
- 已读主会话T03-main-review/T03-bounded-work-review/T03-unknown-fields-review。B负责有限寻路、普通请求不被每轮观察挤占、get_state失败/序号一致性收尾；A负责缺失字段纯层元信息及共享helper测试。仍属当前分配，不重开T02，不自行放行下游。
- FrozenPages::first已集中按整份快照聚合未知字段路径，地面seq/count/name及对象name/hp/max_hp/alive、inventory name；不把结构性空handle/ground uid当未知，分页冻结同一份缺失元信息。OperationUpdate可见message/evidence已按总计划中文化。
- ObservationHandles对已验证且完全相同身份集合快速返回，避免稳定帧重复分配map；变更仍完整验证，新增8192身份稳定100帧/超限/重复身份拒绝用例，记录纯层耗时而不冒充完整游戏帧耗时。新源码增量纯测试进行中。

## 2026-11-01 09:23:04 有界工作收尾

- 共享maxMoveSearchRounds=4096已落盘，B真实MyHero::aiPlanMove使用同值；测试复用真实AStar算法，巨大fake地图在有界次数结束时doneSearch=false，与已证无路doneSearch=true分开。actions18/18通过（0.26秒），随后新增主会话要求的survivor保留回归，待总复验。
- B已将完整观察改为按需refresh，开始单位hop ActionMove/服务器校正清route路径；runtime优先已有操作并保证至少一个普通请求有机会处理，stop/deadline仍优先。仍在完成主会话handle survivor及轻量事件摘要审查，暂不交付冻结manifest。
- 当前本人独占第二轮集成编译，83步；同配置client/server版本保持VENGINEERING-20261101090730，没有重新配置或关闭版本检查。

## 最终交付：MCP-T03-001/r3/delivery1

交付时间：2026-11-01 09:29:07（PowerShell Get-Date，Asia/Shanghai）。负责人01a19251-a370-7491-b33e-f7afef6be379；游戏协作者01a19206-195c-7ce2-a9f8-866c17dc7d18；方案修订3；前置MCP-T02-001/r3/delivery1已正式接受。HEAD仍为b1ddda8d8742afb1d60839f759733a2e8ee26207，T02历史manifest保持不变。

交付manifest：`.runtime/collaboration/T03-delivery-manifest.json`，23个产品文件、10个产物、15份证据的SHA256。B专属报告的16个产品文件逐项重算零差异；报告已保存在`.runtime/collaboration/T03-game-report-snapshot.md`，避免后续报告补记覆盖本次证据。纯API/下游约束见`.runtime/collaboration/T03-api-handoff.md`。任务执行接受/下游分配仍由主会话唯一决定，请审查并重新分配。

### 实际变更

- 新增自包含snapshot.hpp/.cpp、movement.hpp/.cpp及actions测试，CMake将纯源加入mir2x_ai_bridge、真实gameadapter/runtime留在client_objs。负责人未改协作者独占的游戏/runtime文件；协作者未改纯源/CMake/task。
- 当前窗口当前角色接入get_state/list_nearby/get_map_region/get_inventory/move_to五项，连同四内建共九项实际注册能力。没有get_skills/get_interaction或T04/T05 stub，没有改变v1契约/Python固定接口。
- 所有输出经T01闭合schema验证；UID/金币/序号/时间使用十进制字符串，未知name/hp/gold/level和未收到容器保留null/明确缺失元信息。查询使用knownName等纯读取入口；背包覆盖抓取/交易/拍卖暂存并标reserved_count，不把暂存当丢失或自由可用。
- 目标使用真实对象incarnation，地面使用权威更新revision，绑定instance/session/world。离场/重建/替换时失效，survivors保留原handle，旧分页不能复活身份。分页每类最近两份，最多8192行，每页100行，过滤条件/作用域变化或驱逐后cursor明确失败；缺失字段元信息与整份快照一起冻结。地面单格超过100项受控unavailable，不截断。
- 地图区域边界用int64计算，最大33x33，静态walkable来自加载地图，dynamic_blocked仅表示真实视野中的客户端已知占用，视野外null；静态可走不承诺动态可达。
- 移动只用原ClientPathFinder、ActionMove/MyHero分解/CM_ACTION。从currMotion.endX/endY生成路线，先claimIntent，再逐个单位hop；没有Lua、传送、改坐标或伪造服务器回执。实际CM_ACTION send后的序号才标submitted。当前位置达到容差且停止才succeeded/observed_postcondition，证据使用实际位置/动作终点及可选非相关服务端位置。
- 搜索共享上限4096轮；未找到且耗尽返回unavailable，确证无路才unreachable。静态寻路cost不扫描全COList，单位hop避免后续无界长路径。3秒无格进展stalled、30秒截止；busy不替换旧动作；死亡/人工接管/换图/断线沿T02先失效再取消，所有终态释放AI意图。服务端校正使剩余route作废，不重发原目标，仅观察是否已到目标，否则stalled/timeout。
- 运行时先断开、stop和安全deadline；既有动作有剩余预算时先推进，旧pending在新溢出响应前保留一个处理机会。新请求最多8、暂存最多8、每轮响应最多8；完整DTO按需每poll生成至多一次，末尾轻量身份/变化摘要按真实绘制帧合并snapshot事件。安全deadline仍每轮无条件检查，普通工作不能无限延期旧动作。

### 构建与实际测试

同一次最终配置生成`VENGINEERING-20261101090730`，本负责人统一构建client/server及测试；协作者和主会话未操作共享build。首轮140/140完成server和client链接，最终增量83/83完成client及相关测试链接；`t03-up-to-date.log`为`ninja: no work to do.`。没有关闭版本检查。

- `cmake -S . -B build`，配置日志`t03-configure.log`。
- `cmake --build build --target client server test_ai_bridge_actions test_ai_bridge_controller test_ai_bridge_runtime test_ai_bridge_protocol test_ai_bridge_security test_response_callbacks test_netio test_widget --parallel 8`，首轮`t03-build-round1.log`及最终`t03-build.log`。
- `ctest --test-dir build -R '^(ai_bridge_actions|ai_bridge_controller|ai_bridge_runtime|ai_bridge_protocol|ai_bridge_security|response_callbacks|netio|widget)/' --output-on-failure`：最终93/93通过，26.86秒，见`t03-regression-ctest.log`及`t03-lasttest.log`。
- 分组：actions19、controller26、runtime10、protocol13、security11、response_callbacks9、netio3、widget2。T01安全/协议与T08回调回归均在当前产物上运行，不沿用历史日志冒充本轮结果。
- 23产品文件tracked diff及逐文件no-index whitespace检查通过，见`t03-whitespace.log`；该项不是编译或安全测试。

客户端SHA256：`AF7CB2998C0EDB9844FD3478BE4F04ECFA381E7D41ADFD5BF590C0C398F02164`。
服务端SHA256：`17B9E71BE0C25B4E3AEE043BBE39318C83B0993FCD06259E55492B38EA3AC0D9`。

首轮89/89及早期40/40、43/43均为历史开发检查；最终使用本节93/93和manifest。寻路测试最初复用solver导致缓存影响结果，已按真实CreatureMovable每次创建新solver的生命周期修正夹具，保留动态阻挡不可达断言，没有改寻路产品算法或放宽断言。

### A01—A05证据对应及边界

| 验收 | 本轮开发证据 | 尚缺真实证据 |
| --- | --- | --- |
| A01 当前角色/窗口 | 当前ProcessRun/MyHero窄适配、context绑定、client真实链接；角色/地图切换控制器回归 | 登录角色窗口与MCP读移停效果对照 |
| A02 真实已知观察 | null/uint64/方向、未知容器、冻结分页/缺失元信息、句柄失效及survivor、区域溢出；实际纯读取适配编译 | 真实角色状态/周边/背包和GUI暂存对照 |
| A03 移动结果 | 共享AStar可达/墙/动态阻挡/有界耗尽，MoveProgress实际停止/容差/部分动画/服务端纠正/证据，fake控制器实际共享helper闭环 | 实际角色移动动画；其他动作留T04/T05 |
| A04 人工接管 | 19项actions含accepted后接管不执行，T02 controller/runtime门控回归与实际意图终结接入 | 真人窗口动作中接管及续发停止演示 |
| A05 失败与未知 | 未登录、角色/地图/死亡/断线、busy、旧句柄/游标、unreachable与耗尽区分、stalled、30秒、安全deadline不被预算跳过 | 游戏进程中的死亡/跨图/网络纠正场景 |

### 未运行、限制与交接要求

- 当前环境Windows UCRT64；未运行Linux/macOS/POSIX分支。主会话报告Computer Use native pipe不可用，因此没有真实RUN面板/DPI点击/角色移动、死亡过图、人工接管或完整Python→角色效果证据。旧T02四工具冒烟不混作本轮证据。本次相同最终客户端的真实九能力只读互通已由主会话完成，见后述补充；登录角色效果仍由T07汇总。
- 2ms为软预算，单个有界寻路/查询不可抢占。纯helper最终probe：4096轮/64614次cost读取/2071us，8192身份稳定100帧4003us；这些不含真实ProcessRun/GUI成本，不是硬实时或完整帧耗时证明。
- 结果是客户端观察，不是服务端原子快照或相关ack；静态可达不保证动态阻挡可通。资源上限返回unavailable，不隐式缩短目标或静默丢数据。
- 未提交Git、未新增agent、未改共享schema/下载包/用户DB/版本检查。独立服务端/角色开发冒烟由主会话管理，本负责人未启动游戏进程或修改存档。
- 交付时的下游要求：复用handle解析及实际生命周期校验、冻结分页、AI所有权和原正常动作通道；不得复制旧回调或恢复过期目标。当时由主会话接受后分配T04，执行者不自行启动下游；当前接受进展见总计划。

### 2026-11-01 09:30:15 主会话最终客户端真实只读冒烟补充

已读并纳入冻结的T03-client-smoke-report.md。本次实际副本hash与交付client完全相同AF7CB2998C0EDB9844FD3478BE4F04ECFA381E7D41ADFD5BF590C0C398F02164，共同构建标识VENGINEERING-20261101090730；由主会话执行，本负责人没有并发操作其进程。

官方Python MCP SDK initialize与35固定工具/6资源列举通过，真实Windows凭据认证连接实际C++，C++恰9项能力（4内建+本轮5项），无未来stub。七次真实调用覆盖disconnected会话、state/nearby/inventory的not_in_game、events、stop→paused。正常CloseMainWindow后5秒内退出，凭据1→0；未强杀、未宣称游戏ExitCode=0，没有启动server或创建测试DB。原始T03-smoke-process.json、T03-live-observe.json、stderr及T03-smoke-cleanup.json连同冻结报告纳入manifest共15份证据。

此证据补强A01入口绑定、A02未进图反馈、A04停止、A05退出清理的真实互通边界；仍未进入角色，不代表窗口内移动/状态对照/面板点击/人工接管。主会话GUI native pipe不可用限制保持，未改变整体T07验收要求。
