# 玩法配置集中管理评估

评估日期：2026-11-06。当前核对源码基线：`97d7fd049d024f602c6782f9d5ae6124bcf152fa`。状态：研究建议，尚未实施。

## 本期范围与结论

本期只评估打怪升级相关的玩法配置：怪物基础经验、金币与物品掉落、爆率和刷怪；此前要求顺手列出的怪物属性、装备/药水、价格、奖励等作为相邻候选，由用户决定是否纳入。服务器端口、并发容量、线程、构建和部署参数不在本期讨论范围。

用户已经明确目标：将选定的玩法值从源码或脚本数据中抽出，由游戏在启动或地图加载时读取配置，GUI 应用只管理这份配置。运行时加载不要求热更新；可以保存后下次启动/加载生效。修改加载入口是必要工作，不需要因此修改结算算法或新增玩法参数。

采用现成 FastAPI + pywebview + Vue 3/Vite 模板。用户进一步明确 C++ 消费、GUI 易读和结构化需求后，已采纳按领域/对象拆分的标准 JSON、Python Pydantic 校验及 C++ nlohmann/json 加载；模板 YAML/Dynaconf 继续用于工作台自身设置。Lua 从宿主获得纯数据表，继续使用原有算法。实施局部方案与七项任务见 [Plan 修订 2](../plan/20261106-centralized-configuration-catalog.md)，尚未分配产品执行；其他玩法候选仍由用户决定。

本文取代此前“只统一编辑原文件/由 GUI 生成命令行”的目标建议；运行时加载也不应被误认为与“不要求即时生效”冲突。[意图记录](../intent/20261106-centralized-configuration-catalog.md)同步保留范围和待决项。已有更广的[配置系统分析](../../readme/development/configuration.md)与[历史研究](20261104-runtime-gameplay-config.md)继续作为事实资料，不扩大本期范围。

## 就绪模板核对

模板位置：`D:/SourceCodes/mywork/best-practices/templates/python-fastapi-webview`。已只读检查 README、依赖、配置、桌面/HTTP 入口、前端 service/router、打包命令与相关测试源码；未安装依赖、运行 GUI、构建或执行测试。

| 能力 | 模板实际实现 | 本期复用边界 |
| --- | --- | --- |
| 前端 | Vue 3/Vite、TypeScript、Vue Router、Pinia、Axios、Reka UI、Tailwind，页面按 `domains` 组织 | 可承载怪物、掉落、刷怪等业务页面，后续再增加其他业务 |
| 本地业务 API | FastAPI/Uvicorn；生产前端由同一个 HTTP 服务的 `/static/` 提供，开发由 Vite 代理 `/api` | 用于玩法配置查询、校验、差异和保存；desktop 模式绑定 loopback |
| 桌面壳 | pywebview，文档要求 Windows WebView2；桥接系统信息、窗口和通知等能力 | 配置业务统一走服务层；不需要用示例的通用 shell 执行接口来改文件 |
| 配置读取 | `config/config.yaml` → `.env.<profile>` → 系统环境；Dynaconf 合并后由 Pydantic 校验，模型冻结且拒绝未知键 | 这是模板自身配置，不是现成的游戏配置系统；可以复用类型校验与集中读取原则 |
| 加载时机 | `load_settings()` 经 `lru_cache` 缓存并注入应用 | 适合启动快照；不能直接用缓存假装 GUI 重新读到了外部修改或游戏已更新 |
| 发行 | `make binary` 构建前端并用 PyInstaller `--onedir` 打包配置、静态文件和 Python 环境 | 已有桌面发行入口；是目录包，本轮尚未验证运行 |

模板没有玩法配置目录、编辑/保存 API、配置迁移器或游戏 C++/Lua 加载器。它解决工程骨架，后续主要工作在数据模型和消费端接入。无需重新比较 GUI 框架。

## 玩法值在哪里

以下是已核实的类别和关键字段，不冒充逐条完整清单，也不代表用户已经批准迁移全部类别。

| 类别 | 当前来源与字段 | 实际消费及注意点 |
| --- | --- | --- |
| 怪物基础经验 | `common/src/monsterrecord.inc` 的 `.exp`，定义在 `monsterrecord.hpp` | `server/src/monster.cpp` 按伤害占比分配 `getMR().exp`，保留最低值、取整和归属规则 |
| 金币基础掉落 | `server/src/dropitemconfig.inc` 中物品为 `金币（小）` 的行，尤其 `count` | `dropitemconfig.cpp` 继续应用已有 goldRate、金币拆分和随机波动；配置的基数不等于最后实际金币数 |
| 物品爆率和数量 | 同一掉落表的 `group/probRecip/repeat/count` | `probRecip` 是概率分母；`repeat` 展开为多次尝试；group 非零时按组内顺序命中后退出，不能统一解释成互相独立的最终百分比 |
| 地图刷怪 | `server/script/map/*.lua` 的怪物 `name` 与 `loc` 中 `x/y/w/h/count/time` | `map/addmonster.lua::monGener`；time 单位秒，目标数受可行走格数与密度限制，`count=0` 目前不能表示关闭 |
| 现有玩法倍率 | `serverconfigurewindow.fl` 的 dropRate、goldRate、experienceRate | 前两项已有掉落消费者；experienceRate 没有接入经验结算。它们与基数分开列出，不因本期讨论爆率就新增或接通倍率 |
| 怪物战斗基础属性（相邻候选） | `monsterrecord.inc` 的 hp/mp、攻击/防御区间、命中/躲避、恢复、行动间隔等 | 与经验同一记录体系；逐字段核消费者，不能把整条记录全部当作可运行时改写数据 |
| 物品/装备/药水基础值（相邻候选） | `common/src/itemrecord.inc` 的价格、要求、属性及药水数据 | 该文件与 `readme/sql2csv/King_StdItems.csv`、`tools/stditemcsv2cpp` 有生成关系；共享客户端数据的字段需另外核对 |
| 商店/固定奖励（相邻候选） | `server/script/npc/include/merchant/*.lua`、任务脚本、`npc/include/dailyquest.lua` | 固定奖励有金币/经验/物品数量；交易、条件分支和任务状态流程保持代码处理 |
| 地图固定放置（相邻候选） | 地图 Lua 中 `addGuard` 等调用 | 位置/方向可列入数据候选，不与随机刷怪区域混为一类 |

关于金钱，要分别记录怪物金币、任务奖励、物品基础价与商店交易价，不能合成一个无来源的“金币配置”。经验也要区分怪物基础经验、固定任务经验和升级曲线。`SYS_SUMEXP()` 公式及密度计算等逻辑不作为本次基数调整。

## 运行时加载怎样接入

### 怪物经验与相邻属性

`common/src/dbcomid.hpp` 将怪物记录放在 constexpr 数组中，`DBCOM_MONSTERID` 可用于 switch/case，记录顺序决定现有 ID。直接把整份 `monsterrecord.inc` 换成运行时文件会同时影响身份、公共记录和大量消费者，范围明显过大。

建议保留编译期身份、名称/资源关联及现有 ID，只为用户选定的玩法字段建立服务端运行时数据读取入口。现有 `.exp` 的消费点改为读取已校验的经验数据，伤害占比分配公式不变。相邻战斗属性只有经逐字段核对后再决定是否采用相同方式；不能据此承诺全部字段都仅服务端使用。

迁移时从现有权威数据导出初始值，并对纳入范围的字段明确覆盖完整性。正式迁入后只维护一处数值权威；不长期让 `.inc` 和外置 JSON 各自可编辑，也不在非法配置时悄悄退回旧值。未迁移字段继续保持原有来源。

### 金币与物品掉落

`getMonsterDropItemConfigList()` 当前在首次调用时从 `.inc` 建立静态索引，按 `repeat` 展开条目；`getMonsterDropItemList()` 再抽样并产生物品。运行时方案可以将“从 `.inc` 构表”换为“启动时读取配置构表”，保留后续数据结构和抽样计算。

迁移必须保留怪物/物品引用、group、probRecip、repeat、count 以及组内顺序。不能在 GUI 排序后直接重排权威列表；不能把 `repeat` 展开后又保存为同一来源而丢失原始字段。表头注明旧输入来自外部 MonItems，本仓库有生成器但未发现该原输入，初始迁移需明确采用哪份现存数据。

当前 node 有效性检查会过滤 probRecip/repeat/count 不满足条件的行；字段注释中的“0 表示禁用”与实际过滤行为要一并呈现。是否接受零值及如何显示，应以保持现有行为和用户选择为准，不能在配置迁移中另造关闭语义。

### 地图刷怪与固定奖励

地图 Lua 将字面量表传给 `addmon.monGener`，适合把该表抽出为外部玩法数据，再在地图脚本加载时取回数据表，继续调用原函数。可由 C++ 统一加载后传给 Lua，不必另加一套 Lua JSON 解析器。具体宿主接口尚未实现。

`monGener` 会向传入表写入 uidList、lastUpdateTime、gridCount 等状态并可能删除条目，因此每个地图实例必须得到独立的可变运行副本，不能共享或写回持久配置快照。原密度限制、最低数量和周期判断保持不变。

固定奖励同理只外置奖励数据，领取条件和发放流程保留 Lua。若奖励描述中也写死了“1000金币”等文本，应列为关联字段并供用户决定处理方式，不能数值变了却仍显示旧奖励。

## 用什么配置系统

推荐本地文件配置，不需要引入独立的分布式配置中心。按玩法领域拆分、统一校验与保存，比把所有内容塞进一个大文件更容易维护。

| 方案 | 本期适合之处 | 代价与结论 |
| --- | --- | --- |
| JSON + Python 类型校验 + C++ nlohmann/json | 类型、对象和数组明确；Python 标准库/Pydantic、Vue/TypeScript 都便于处理；列表顺序明确 | 当前优先推荐。无标准注释，由管理元数据和 GUI 承载说明；原生仍需明确类型、范围和引用校验 |
| YAML + Python 类型校验 + C++ 原生读取 | 嵌套数据可读、支持注释，贴合模板自身设置 | 仍可行；跨语言须约束隐式类型、别名和扩展语义，注释保真写回另需处理。模板使用 YAML 不要求游戏采用同一格式 |
| TOML | 标量和少量配置段可读，Python 有标准库读取 | 大量掉落/刷怪嵌套表较冗长，GUI 写回仍需写入库，本期不是首选 |
| SQLite | 本地事务和查询方便 | 游戏消费、版本控制 diff、数据导入发布增加工作；本期没有据此选数据库的需求 |
| 继续编辑 CSV/Lua/.inc 或生成源码 | 原来源与生成链仍可作为迁移输入或未迁移项适配 | 可做盘点和迁移工具，单靠此方案不能完成已选玩法字段的运行时加载目标 |

Python 侧使用标准库 JSON 读取与模板 Pydantic 风格，负责字段类型、边界和跨字段/引用检查。Dynaconf 保留为工作台自身设置的加载工具，不是游戏运行时，也不会替 C++/Lua 自动读取文件。玩法数值以选定文件为权威，不默认开放全部环境变量覆盖，以免 GUI 保存值与实际加载值无从对应。

C++ 读取相同 JSON，校验后将其转换为有类型的结构体/容器并建立只读快照；游戏计算不逐次查找 JSON 键。校验必须在游戏侧存在，不能假定用户只通过 GUI 修改文件。UTF-8、概率分母、列表顺序、整数范围及未知引用在两端一致。复杂跨字段校验不能只靠 JSON Schema 自动完成，仍由领域校验实现。

标准 JSON 统一格式化写回，不承诺保留手工注释，也不引入 JSONC/JSON5。说明、单位、约束和字段状态由管理元数据与 GUI 展示。使用数组保留有业务语义的顺序；object 用于固定字段，不以对象键名或迭代顺序表达掉落规则。

建议目录的概念示例（不是已新增的接口或已选定的字段）：

```text
server/config/gameplay/
  manifest.json             # 文件版本和领域索引
  monsters.json             # 选定的基础经验/怪物数值，按现有 ID
  drops/<monster_id>.json    # 每只怪物的有序掉落条目
  spawns/<map_id>.json       # 每张地图的怪物及区域数据
  catalog.json              # 只读身份目录，供 GUI 使用
```

文件按领域拆分并不妨碍统一 GUI。不同游戏节点必须使用相同版本的玩法数据；发布方式、缺失文件策略和配置发现路径在实施计划中确定，本期不扩展讨论服务端口或部署管理功能。

## GUI 和游戏各自负责什么

```text
Vue 表单 -> FastAPI 业务服务 -> 校验/差异/保存 -> 玩法配置文件
                                                   |
                                      游戏启动/地图加载时读取
                                                   |
                                      既有经验/掉落/刷怪算法
```

GUI 只负责管理数据，游戏直接消费配置文件，两个方向通过同一份权威数据连接。GUI 未运行也能启动游戏；GUI 保存不自动重启游戏、不代表运行中快照已改变。前端页面不解析 C++ 或执行 Lua 来探测值。

保存前检查外部修改，先校验完整候选、预览差异，再写文件。游戏只使用完整校验成功的快照；不支持热更新的阶段在启动前完成编辑，多文件保存逐项报告，不宣称具备跨文件事务。文件格式与布局由后续计划确定。

## 下一步评估与可检查结果

用户已采纳方案并要求拆分。[Plan 修订 2](../plan/20261106-centralized-configuration-catalog.md)建立七项已接受局部方案：数据契约/基线、原生加载/经验、掉落、刷怪、Python 管理服务、Vue/桌面界面、联合检查。前置交付接受后才能放行下游，当前已按用户授权实施，执行状态与实际检查以总计划及验证记录为准；下列调研计数是迁移前事实。

后续实现的主要验收候选：迁移初值与现有数据逐字段一致；怪物/物品 ID 不变；相同数据下经验分配、掉落分组/顺序和补怪语义保持；保存后独立启动游戏可以加载而不依赖 GUI；非法值/未知引用启动即报错；配置保存与实际生效状态分开。未做窗口、网络、运行服或角色验证，不把源码阅读当作产品已实现。

## 源码证据

- [怪物记录结构](../../common/src/monsterrecord.hpp)、[现有怪物数值](../../common/src/monsterrecord.inc)、[编译期身份与记录](../../common/src/dbcomid.hpp)、[经验分配](../../server/src/monster.cpp)。
- [掉落源表](../../server/src/dropitemconfig.inc)、[构表与抽样](../../server/src/dropitemconfig.cpp)、[掉落导入工具](../../tools/dropgen/src/dropgen.py)、[已有倍率字段](../../server/src/serverconfigurewindow.fl)。
- [地图加载](../../server/src/servermap.cpp)、[刷怪消费](../../server/script/map/addmonster.lua)、[边境城市刷怪数据](../../server/script/map/边境城市_01.lua)。后续只读词法核对为 255 个 Lua 中有 251 份地图实际调用 monGener，helper 中的函数定义另计；2,082 个怪物组、3,328 个区域均可按字面量读取，但仍不能代替实际生产消费验证。
- [物品记录](../../common/src/itemrecord.inc)、[物品 CSV](../../readme/sql2csv/King_StdItems.csv)、[物品生成器](../../tools/stditemcsv2cpp/src/stditemcsv2cpp.py)、[怪物 CSV](../../readme/sql2csv/King_Monster.csv)、[怪物生成器](../../tools/monstercsv2cpp/src/monstercsv2cpp.py)。
- [固定奖励](../../server/script/npc/include/dailyquest.lua)。商店、其他任务与地图固定放置尚需继续逐项盘点，不宣称已形成完整目录。
- 外部模板证据：`src/example_api/config.py`、`desktop.py`、`api.py`、`server.py`、`web/package.json`、`web/src/services/`、`web/vite.config.ts`、`Makefile` 及现有 config/profile/runtime/desktop 测试源码。
