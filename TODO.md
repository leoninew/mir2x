<a id="small-tasks"></a>
# 小任务
- 支持 NPC 消失、变身、说话，以及怪物或玩家外形的 NPC。
- 魔法攻击应在火球、冰球等魔法爆炸时向服务端报告完成，而不是施法时。
- 困魔咒空间中可交互的破墙。
- 进一步理解地图格子触发器。
- 怪物和魔法的配置、编辑界面。
- 跟随 UID 的魔法在追踪时应消失。

<a id="honchondo--honchonmado--the-two-legendary-blade-questlines"></a>
# HonChonDo / HonChonMado：两条传说之刃任务线

以下是为转换 `Envir/QuestDiary/HonChonDo` 和 `Envir/QuestDiary/HonChonMado` 所做的调研。**调研时尚未转换任何内容。** 这不像此前 23 个 `MU_*` 技能任务那样只需转写，首先需要 6 至 8 项新的引擎能力，因此是一个功能开发项目。

调研写于 2026-09-04，依据当时的 `Envir.utf8` 和 `quest` 分支；其中的实现状态和文件数量在使用前需要重新核对。

---

<a id="1-what-it-is"></a>
## 1. 任务概况

这是关于传说之剑及其影子的故事，分为两部分：

| 部分 | 刀剑 | 阶段数 | 文件数 | 大小 |
|---|---|---|---|---|
| `HonChonDo` | **寂幻之刃**（影子） | 25 | 15 | 约 190 KB |
| `HonChonMado` | **影魅之刃**（真剑） | 11 | 16 | 约 79 KB |

共 31 个文件，约 270 KB，与此前技能任务转换的全部内容规模相当。

`HonChonDo` 是前置任务：玩家从中获得寂幻之刃；阶段 1-22 说明疯狂勇士将其借出**一周**，以考验玩家是否有资格持有影魅之刃。之后 `HonChonMado` 继续追寻真剑，最后对战堕落的**诺玛教主之魂**。

<a id="honchondo-stage-list-convert_defquestdiaryhonchondohelperhonchondobooktxt"></a>
### HonChonDo 阶段列表（`Convert_Def/QuestDiary/HonChonDo/HelperHonChonDoBook.txt`）

游戏内任务日志共 25 节 `@Convert_HelperHonChonDoBook_0..24`；`_0` 表示“尚未开始”。

| 阶段 | 标题和内容 | 所需物品 |
|---|---|---|
| 1-1 | 诺玛们的宗教活动：毁灭之印只在诺玛仪式期间出现 | – |
| 1-2 | 4条道路：仪式受到守卫，只有四条接近路线 | – |
| 1-3 | 获取宝箱：拿到宝箱后交给夏柯 | – |
| 1-3 | 开启宝箱：在夏柯面前打开宝箱 | 宝箱 |
| 1-4 | 请求调查遗址 4楼：夏柯请求调查遗址四层 | – |
| 1-5 | 用尸骨做成的项链：尸骨项链是已故诺玛勇士的遗物，向拉贝卡询问 | 尸骨项链 |
| 1-6 | 与拉贝卡的见面：他留在教主身边调查扎马尔之死，帕斯卡知道更多 | 尸骨项链 |
| 1-7 | 寻找帕斯卡：地下神女只在试炼后交谈 | 尸骨项链 |
| 1-8 | 击退护身符：梅山侠送来护身符帮助通过试炼 | 击退护身符 |
| 1-9 | 与帕斯卡的对话：她告知其位置 | 击退护身符、尸骨项链 |
| 1-10 | 与帕斯卡的对话：阿龙怪打断对话 | 击退护身符、尸骨项链 |
| 1-11 | 勇者的儿子：阿龙怪可能是勇士的儿子 | 击退护身符、尸骨项链 |
| 1-12 | 击退护身符：护身符将其驱走，但项链遗失；向夏柯报告 | – |
| 1-13 | 夏柯的新请求：回去向帕斯卡了解更多情况 | – |
| 1-14 | 地下神女的安排：她安排第二次会面 | – |
| 1-15 | 重新出现：阿龙怪返回，帕斯卡未能说服他 | – |
| 1-16 | 与帕斯卡的对话：等他离开后继续 | – |
| 1-17 | 听到秘密：得知秘密 | – |
| 1-18 | 报告夏柯 | – |
| 1-19 | 诺玛圣地：潜入诺玛圣地寻找毁灭之印 | – |
| 1-20 | 永远的平行线：玩家与阿龙怪无法合作 | – |
| 1-21 | 发疯的诺玛勇士之魂：与绑定在寂幻之刃上的勇士交谈 | 寂幻之刃 |
| 1-22 | 魔剑的影子：勇士将寂幻之刃借出**一周**作为考验 | – |
| 1-23 | 寂幻之刃：缺少四只眼睛，只是影魅之刃的影子，但仍是强力武器 | – |

<a id="honchonmado-stage-list-convert_defquestdiaryhonchonmadohelperhonchonmadobooktxt"></a>
### HonChonMado 阶段列表（`Convert_Def/QuestDiary/HonChonMado/HelperHonChonMaDoBook.txt`）

共 11 节 `@Convert_HelperHonChonMaDoBook_0..10`。

| 阶段 | 标题和内容 |
|---|---|
| 1-1 | 影魅之刃的诱惑：收集成为剑之眼的项链 |
| 1-2 | 艰难的出发：万里碧海遗失，旅程刚开始就受阻 |
| 1-3 | 魔达的帮助：魔达解释如何破坏机关并获得信物 |
| 1-4 | 其他的动向：匿名信暗示有竞争对手 |
| 1-5 | 神秘之印：紫霞神女交出打开第三守护者大门的印记 |
| 1-6 | 守护石和守护者：随着剑接近完成，旧秘密和不祥预兆出现 |
| 1-7 | 怪人的人情：从怪人处获得大脚将的角笛，前往祭祀长领地 |
| 1-8 | 魔达的凶计：魔达在最后一道门前露出真面目 |
| 1-9 | 疯了的诺玛教主的灵魂：击败教主堕落之魂，试炼结束 |

<a id="cast"></a>
### 登场人物

`夏柯`（Sahaka）、`巴斯卡/帕斯卡`（Baska）、`扎马尔/자하`（Jaha）、`拉贝卡`、`玄`（Hyun）、`魔达`（Mado，叛徒）、`紫霞神女`、`地下神女`、`梅山侠`、`诺玛大法老1..4`、`阿龙怪1..4`、`诺玛教主`（最终首领）、`诺玛司令`。

---

<a id="2-it-is-a-scheduled-world-event-not-a-player-driven-quest"></a>
## 2. 这是定时世界事件，不是玩家驱动的任务

这是它与技能任务最大的结构性差异，也是不能照搬技能任务转换方式的原因。

`HonChonDo/HonMainControl.txt` 持续重复调度，并根据**现实日历**选择分支：

```
[@MainHonChonRootin]
DelayGoto [Grobal] 180 @Honchonmadocontrol 1005      -- re-fire every 180s, forever

[@DAYOFWEEKCHECK]
DAYOFWEEK TUE  -> @tuesdayHonQuest
DAYOFWEEK WED  -> @wednesdayHonQuest
DAYOFWEEK THU  -> @thursdayHonQuest
DAYOFWEEK FRI  -> @fridayHonQuest
DAYOFWEEK SAT  -> @saturdayHonQuest
```

两个目录中共有 38 次 `DAYOFWEEK` 检查和 144 个 `HOUR`/`MIN` 时间窗口。`HonMainControl.txt` 中的具体时间安排：

- **周二 23:01–23:05**：`@tuesdayMonGen`，生成诺玛仪式。
- **周三**：01/03/05/07/09/11/13/15/17/19 点的 :01–:05 再次执行 `@tuesdayMonGen`；22:01 和 23:01 执行 `@wednesdayMonGen`。
- **周四**：03/08/14/19 点的 :01–:05 执行 `@wednesdayMonGen`；20:55–20:59 执行 `@wednesdayMonClear`；22:00–23:50 执行 `@thursdayMonGen`；23:50–23:59 执行 `@thursdayMonClear`。
- **周五 00:01–00:05**：法老从候场区移入遗址。
- **周五 20:55–20:59**：法老移回候场区。
- **周五 21:01–21:05**：4 个阿龙怪从遗址移入候场区。
- **周五 23:01–23:05**：法老移到 `D1604`。
- **周六 22:55–22:59**：法老返回候场区。

> `b82f2bee` 中添加的游戏内昼夜时钟（`isDayTime`/`isNightTime`，依据 `uptime()`，白天和黑夜各 1 小时）**不能**满足这里的需求。那是 2 小时游戏周期，而这里需要现实中的星期和时刻；不要混淆两种时钟。

<a id="npc-relocation"></a>
### NPC 跨地图移动

39 次 `Movenpc` 调用在候场区和地牢之间移动 NPC：

```
Movenpc "npcroom1,10,7,诺玛大法老1"  D16031 61 354
Movenpc "npcroom1,10,8,诺玛大法老2"  D16032 374 371
Movenpc "npcroom1,10,9,诺玛大法老3"  D16033 369 46
Movenpc "npcroom1,10,10,诺玛大法老4" D16034 30 30
Movenpc "D16031,61,355,阿龙怪1"      npcroom1 9 7
```

**mir2x 已经在完全相同的坐标放置了 8 个占位脚本**，候场区设计已被带入，但尚未接通：

```
NPC屋_NPCROOM1.诺玛大法老_1.GLOC_10_7_0.LOOK_86.lua   <->  "npcroom1,10,7,诺玛大法老1"
NPC屋_NPCROOM1.诺玛大法老_2.GLOC_10_8_0.LOOK_86.lua   <->  "npcroom1,10,8,诺玛大法老2"
NPC屋_NPCROOM1.诺玛大法老_3.GLOC_10_9_0.LOOK_86.lua   <->  "npcroom1,10,9,诺玛大法老3"
NPC屋_NPCROOM1.诺玛大法老_4.GLOC_10_10_0.LOOK_86.lua  <->  "npcroom1,10,10,诺玛大法老4"
NPC屋_NPCROOM1.阿龙怪_1.GLOC_9_7_0.LOOK_90.lua        <->  "npcroom1,9,7,阿龙怪1"
NPC屋_NPCROOM1.阿龙怪_2.GLOC_9_8_0.LOOK_90.lua        <->  "npcroom1,9,8,阿龙怪2"
NPC屋_NPCROOM1.阿龙怪_3.GLOC_9_9_0.LOOK_90.lua        <->  "npcroom1,9,9,阿龙怪3"
NPC屋_NPCROOM1.阿龙怪_4.GLOC_9_10_0.LOOK_90.lua       <->  "npcroom1,9,10,阿龙怪4"
```

这 8 个都是**空文件**，属于当时仓库中 191 个空 NPC 占位文件的一部分；同类文件曾使 `24a16db5` 中的 U+F00D 文件名问题难以察觉，需要留意。

`ServerMap::loadNPChar` 在加载地图时根据文件名生成 NPC。当时没有跨地图移动 NPC 的操作，因此要么实现该操作，要么放弃候场区设计，为每个 NPC 编写只在规定时间窗口响应的脚本。

---

<a id="3-world-state-and-the-unique-blade"></a>
## 3. 世界状态与唯一的剑

`TBL_HonChonMadoSeverFlag` 的键是**单条具名记录**，不是玩家。`Sever` 而非 `Server` 是原始资料自身的拼写错误：

```
FormatStr "FLD_MADOQUEST='%s'" "HonChon"
ReadValueSql "TBL_HonChonMadoSeverFlag" %A9 "FLD_MADOHAVE" [@callback]
```

`FLD_MADOHAVE=1` 表示**游戏世界中已有玩家持有寂幻之刃**。根据 `HonChondonpc.txt`，交付剑时执行：

```
TAKE 毁灭之印 1
Givew 1 寂幻之刃                                       -- straight into wear slot 1
UpdateValueSql "TBL_HonChonMadoQuest"     %A9 "FLD_MADOMAIN=7"
UpdateValueSql "TBL_HonChonMadoQuest"     %A9 "FLD_MADOCONTENT=1"
UpdateValueSql "TBL_HonChonMadoQuest"     %A9 "FLD_MADOQUESTCHECK=1"
UpdateValueSql "TBL_HonChonMadoSeverFlag" %A9 "FLD_MADOHAVE=1"
set [205] 0
Eventmsg grobal %A7 2000                               -- announce it to the whole server
```

因此这把剑**全服唯一，借期一周**（阶段 1-22）。当 `FLD_MADOHAVE=1` 时到达的其他玩家会进入 `@HonChondo_Questnpc_1_have` 分支。

仅按玩家保存的 `fld_*` 任务模型无法表达这一点，需要共享任务状态和基于现实时间的到期机制。

---

<a id="4-the-four-sql-tables"></a>
## 4. 四张 SQL 表

**原始数据没有附带表结构**：在整个 `Envir` 中执行 `grep -i "create table"` 没有结果。这些表由原版服务器或管理员另外创建，因此只能从调用位置推测列类型。

使用 `ReadValueSql` / `UpdateValueSql` 操作 `TBL_*` 是**原版通用能力**，不是这两条任务线独有；`QuestDiary/CastleWar/` 也以相同方式使用 `TBL_SnakeVallyWar`。因此调研时认为 mir2x 的缺口是“完全没有脚本级 SQL”，一组读写绑定和异步回调约定可服务于所有使用该机制的原版任务线。

<a id="tbl_honchonmadoquest--per-player-honchondo-half"></a>
### `TBL_HonChonMadoQuest`：按玩家保存 HonChonDo 进度

键为 `FLD_MADOUSER='<username>'`。进度分三级：`FLD_MADOMAIN`（章节）、`FLD_MADOCONTENT`（阶段）、`FLD_MADOQUESTCHECK`（子阶段）。

| 列 | 说明 |
|---|---|
| `FLD_MADOUSER` | 用户名，作为键 |
| `FLD_MADOMAIN` | 章节，曾观察到设置为 7 和 8 |
| `FLD_MADOCONTENT` | 阶段 |
| `FLD_MADOQUESTCHECK` | 子阶段 |
| `FLD_MADOPATTERN` | 出现在插入列列表中；**需检查是否有读取方** |
| `FLD_MADOTEMPQUEST` | 出现在插入列列表中；**需检查是否有读取方** |

记录在 `HonChonDo/MadoNpcSahaka.txt:37` 创建一次。11 个 `QuestDiary/HonChonDo/*.txt` 文件中共有 323 处引用，最多的是 `Mon_extinction.txt`（61）、`MadoNpcSahaka.txt`（57）、`MadoNpcBaska.txt`（51）、`MadoNpcJaha.txt`（47）。目录外还有 5 处引用：`QuestDiary/NpcQuest_Def/17baska1-npcroom1.txt` 至 `17baska4-npcroom1.txt` 和 `17Zaha_Numa-41.txt`；**不要遗漏这些候场区 NPC 脚本。**

<a id="tbl_madohaveuserquest--per-player-honchonmado-half"></a>
### `TBL_MadoHaveUserQuest`：按玩家保存 HonChonMado 进度

键为 `FLD_MADOHAVEUSER='<username>'`。列包括 `FLD_MADOUSERCONTENT`（阶段 1..13 及以上）、`FLD_MADOUSERCHECK`（子阶段）、`FLD_MADOUSERPATTERN`。

记录在 `HonChonMado/MadoNpc.txt:37` 和 `SahakaNpc.txt:28` 创建。全部 16 个 `QuestDiary/HonChonMado/*.txt` 文件中共有 165 处引用，最多的是 `Mon_MadoHave.txt`（46）、`MonsterNpc1.txt`（27）、`MadoNpc.txt`（21）。

Helper 任务书通过 `Equal D0 n` / `Large D0 n` / `Small D0 n` 按 `FLD_MADOUSERCONTENT` 分支，最高到 `Large D0 13`；因此虽然日志只有 11 项，阶段计数仍会超过 13。

<a id="tbl_honchonmadoseverflag--server-wide-singleton"></a>
### `TBL_HonChonMadoSeverFlag`：全服单例

键为 `FLD_MADOQUEST='HonChon'`。列包括 `FLD_MADOHAVE`、`FLD_MADOMAPEVENT`、`FLD_MADOQUESTSET`。共有 29 处引用：`MadoNpcJaha.txt`（10）、`extinctionQuestEvent.txt`（9）、`MadoNpcBaska.txt`（8）、`HonChondonpc.txt`（2）。

<a id="tbl_honchonmadoserver--one-reference"></a>
### `TBL_HonChonMadoServer`：一处引用

位于 `HonChonDo/MadoextinctionNpc.txt`，只有 `FLD_PASSWORDNAME` 一列。用途尚不清楚，可能是 GM 或管理员权限门槛；实现前应先调查。

<a id="access-pattern"></a>
### 访问方式

SQL 均通过字符串拼接生成，读取操作**异步执行并使用回调标签**：

```
FormatStr "FLD_MADOHAVEUSER='%s'" %USERNAME
ReadValueSql "TBL_MadoHaveUserQuest" %A9 "FLD_MADOUSERCONTENT,FLD_MADOUSERCHECK,FLD_MADOUSERPATTERN" [@MadoNpc_Call_0]

[@MadoNpc_Call_0()]
mov D1 %ARG(1)
mov D2 %ARG(2)
mov D3 %ARG(3)
```

mir2x 的 Lua 协程（`bindCoop` + `LuaCoopResumer`）适合表达这一流程：让出执行权的 `dbQuestRead(...)` 看似同步，比回调标签更易读。

两个目录中的命令总数：**119 次 `ReadValueSql`、3 次 `WriteValueSql`（插入记录）、401 次 `UpdateValueSql`**。

---

<a id="5-complete-legacy-verb-inventory"></a>
## 5. 原版命令清单

下表列出两个目录中每行开头的命令及调研时 mir2x 的支持情况。统计命令为 `cat HonChonDo/*.txt HonChonMado/*.txt | grep -oE "^[A-Za-z_]+" | sort | uniq -c`。

| 命令 | 次数 | mir2x | 说明 |
|---|---|---|---|
| `Equal` / `Small` / `Large` | 812/16/14 | ✅ | 普通 Lua 比较 |
| `break` / `goto` | 742/581 | ✅ | 控制流，转换为 Lua 结构 |
| `UpdateValueSql` | 401 | ❌ | **无脚本级 SQL** |
| `FormatStr` | 308 | ✅ | `string.format` |
| `LoadValue` | 288 | ✅ | 外置字符串；转换时将文本内联 |
| `Eventmsg` | 270 | ❌ | 广播；`Eventmsg grobal <msg> <ms>` 面向全服 |
| `Delaygoto` / `DelayGoto` | 231/10 | ✅ | `runQuestThread` + `pause`，可取消 |
| `mov` / `Mov` / `movr` / `inc` / `AddStr` | 277/7/27/28/3 | ✅ | Lua 局部变量；`movr` 对应 `math.random` |
| `Checkitemw` / `Checkitem` / `checkitem` | 155/49/88 | ✅ | `getWLItem`, `hasItem` |
| `HOUR` / `min` / `MIN` / `Min` | 144/23/6/2 | ❌ | **现实时间的小时和分钟** |
| `ReadValueSql` | 118 | ❌ | **无脚本级 SQL** |
| `set` | 66 | ✅ | 任务标记 |
| `take` / `Take` / `takew` | 47/2/1 | ✅ | `removeItem`, `removeWearItem` |
| `Mongenp` | 46 | ⚠️ | 已有 `addMonster`，但**不能覆盖单次刷怪的掉落表** |
| `Movenpc` | 39 | ❌ | **将 NPC 移至另一张地图** |
| `ApplyMonEx` | 38 | ❌ | **怪物阵营或称号标记**（`[Monsterside]`） |
| `DAYOFWEEK` | 35 | ❌ | **现实星期** |
| `monclear` / `MonClear` | 35/22 | ✅ | `clearMonster()`，由 `b82f2bee` 添加 |
| `give` / `Give` / `givew` | 35/5/1 | ⚠️ | 有 `addItem`；`Givew <slot> <item>` 直接放入装备槽的操作没有绑定，C++ 的 `Player::setWLItem` 当时位于 `player.cpp:2146` |
| `linemsg` / `Linemsg` | 11/6 | ❌ | 向单张地图广播 |
| `Checkmonmap` / `checkmonmap` | 11/6 | ✅ | `getMonsterCount()` |
| `Enter_fail` | 10 | ✅ | `setupMapGridTrigger` / `setupMapUIDGridTrigger`，返回 `false` |
| `close` | 9 | ✅ | 事件中的 `close="1"` |
| `mapting` | 8 | ❓ | `mapting D1606 41 148 84`：**4 个参数，用途未知**，需调查 |
| `mapmove` / `MAPMOVE` / `map` | 5/3/5 | ✅ | `spaceMove` 自 `b82f2bee` 起接受名称、ID 或 UID |
| `return` | 4 | ✅ | |
| `GiveExp` | 4 | ⚠️ | `Player::gainExp` 当时位于 `player.cpp:1822`，**未绑定** |
| `gender` | 3 | ✅ | `getGender()`，已修复原来错误的 `getGener` |
| `WriteValueSql` | 3 | ❌ | **无脚本级 SQL** |
| `GroupMove` | 3 | ❌ | 移动整个队伍：`GroupMove "D16062,37,48" %USERNAME` |
| `mapspell` | 2 | ⚠️ | `mapspell [firewall] "D16061,22,33,40,47, +d0, +t10, +r50, +h"`；已有 `ServerMap::MapGrid::fireWallList`（当时位于 `servermap.hpp:87`），但没有 Lua 绑定 |
| `checkjob` / `checkgold` / `Checkhum` / `Check` | 2/2/2/2 | ✅ | |
| `Checkweaponlevel` | – | ❌ | `HonChondonpc.txt` 中的 `!Checkweaponlevel 1` |

<a id="summary-of-what-must-be-built"></a>
### 所需能力汇总

1. **现实时间调度器**：星期、小时和分钟窗口，以及全服重复计时（原版为 `DelayGoto [Grobal] 180`）；与游戏内昼夜时钟分开。
2. **脚本级 SQL**：读取、插入、更新，其中读取使用协程形式；攻城战也可受益。
3. **共享的非玩家任务状态**：`Sever`Flag 单例，以及剑借出一周的现实时间到期机制。
4. **`Movenpc`**：运行时将 NPC 移到另一张地图。
5. **广播**：`Eventmsg grobal` 为有持续时间的全服广播，`linemsg` 为单地图广播。
6. **`Mongenp`**：刷怪时覆盖该次生成的掉落表。
7. **`ApplyMonEx [Monsterside]`**：怪物阵营或称号，使事件怪物体现所属阵营。
8. **`GroupMove`**：传送整个队伍。
9. 为已有 C++ 能力补充绑定：`gainExp`、`setWLItem`（`Givew`）、供 `mapspell` 使用的火墙绑定，以及 `Checkweaponlevel`。
10. 查明 `mapting` 的用途。

---

<a id="6-maps"></a>
## 6. 地图

| 原版 | mir2x | ID |
|---|---|---|
| `D1604` | 诺玛遗址4层_D1604 | 293 |
| `D16031` | 诺玛遗址3层_D16031 | 289 |
| `D16032` | 诺玛遗址3层_D16032 | 290 |
| `D16033` | 诺玛遗址3层_D16033 | 291 |
| `D16034` | 诺玛遗址3层_D16034 | 292 |
| `D1606` | 诺玛勇士墓穴_D1606 | 295 |
| `D16061` | **缺失** | – |
| `D16062` | **缺失** | – |
| `D16063` | 怪人屋_D16063 | 296 |
| `D16064` | 祭祀长领地_D16064 | 297 |
| `npcroom1` | NPC屋_NPCROOM1 | – |

`D16061` 和 `D16062` 没有 `maprecord.inc` 条目。`D16061` 被使用 43 次，是 `mapspell [firewall]` 陷阱所在地图，也是 `GroupMove` 的目标；`D16062` 被使用 3 次。在认定可以添加之前，应检查对应地图数据是否存在。

其他引用包括通过 `mapting` 使用的 `D16062 41 148 84`，以及 `mapting D1606 4 174 139`。

<a id="7-items"></a>
## 7. 物品

`give` / `take` / `checkitem` 共指定 21 种物品。**其中 15 种在 `itemrecord.inc` 中缺失。**

| 物品 | 数据库中存在 |
|---|---|
| 影魅之刃 | ✅ |
| 寂幻之刃 | ✅ |
| 万里碧海 | ✅ |
| 九宫云雾 | ✅ |
| 血花落照 | ✅ |
| 黑天暗云 | ✅ |
| 击退护身符 | ❌ |
| 大地石 | ❌ |
| 大族长角笛 | ❌ |
| 太阳石 | ❌ |
| 安息石 | ❌ |
| 尸骨项链 | ❌ |
| 心石 | ❌ |
| 无名书 | ❌ |
| 月光石 | ❌ |
| 机关零件 | ❌ |
| 毁灭之印 | ❌ |
| 活石 | ❌ |
| 神秘之印 | ❌ |
| 藏宝箱 | ❌ |
| 青空石 | ❌ |

两把剑本身都存在；所有任务信物和五种元素石（大地石/太阳石/心石/活石/青空石）都不存在。日志同时提到 `宝箱`（存在）和 `藏宝箱`（缺失），需确认原版指的是一个物品还是两个。

**在 `itemrecord.inc` 末尾追加不会改变已有 ID**：ID 取决于位置，`s_itemRecordAssertor` 在启动时检查名称唯一性。删除或在中间插入则不具备这一保证。

<a id="8-monsters"></a>
## 8. 怪物

| 怪物 | 记录 | NPC 占位 |
|---|---|---|
| 诺玛司令 | ✅ | – |
| 诺玛教主 | ✅ | – |
| 诺玛大法老 | ✅ | 4 个空文件 |
| 阿龙怪 | ❌ | 4 个空文件 |

`阿龙怪` 既是候场区的 NPC，**也是**怪物（`Mongenp "D16031,33,44,10" 阿龙怪 1`），因此除 NPC 脚本外还需要怪物记录。

`extinctionMongen.txt` 中的生成示例：

```
Mongenp   "D16031,33,44,10" 诺玛司令 20 "noitem,金创药（特）,魔法药（特）"
ApplyMonEx "D16031,33,44,30" 诺玛司令 [Monsterside] "毁灭之印大师"
Mongenp   "D16031,33,44,10" 阿龙怪  1  "noitem,金币,5000,金创药（特）,魔法药（特）"
ApplyMonEx "D16031,33,44,30" 阿龙怪  [Monsterside] "???"
```

`"noitem,..."` 只覆盖该次生成的掉落表。`ApplyMonEx [Monsterside] "<title>"` 为怪物添加标记；携带印记的诺玛司令使用 `毁灭之印大师` 称号。

<a id="9-flags"></a>
## 9. 标记

- `[205]`：HonChonDo 已激活；交付剑时设为 0。
- `[207]`：HonChonMado 已激活；控制 Helper 任务书的访问。
- `[208]`：在 `MadoNpc.txt` 中与 `[207]` 一起设为 0。

具体进度保存在 SQL 中，不保存在这些标记里；标记只表示“任务线是否开放”。

<a id="10-file-map"></a>
## 10. 文件地图

<a id="questdiaryhonchondo-15-files"></a>
### `QuestDiary/HonChonDo`（15 个文件）

| 文件 | 字节数 | 内容 |
|---|---|---|
| `extinctionQuestEvent.txt` | 43536 | 最大的文件；29 处 `TBL_HonChonMadoQuest`、9 处 `SeverFlag` 引用，38 次 `UpdateValueSql` |
| `MadoNpcBaska.txt` | 35981 | 帕斯卡/巴斯卡对话，51 + 8 处引用 |
| `MadoNpcSahaka.txt` | 20055 | 夏柯；**创建 `TBL_HonChonMadoQuest` 记录**（第 37 行） |
| `Mon_extinction.txt` | 15087 | 怪物死亡钩子，61 处引用、39 次更新 |
| `MadoNpcJaha.txt` | 13279 | 扎马尔/자하，47 + 10 处引用 |
| `MadoNumaNpc.txt` | 11852 | 诺玛 NPC，24 处引用 |
| `MadoextinctionNpc.txt` | 9149 | 25 处引用；唯一使用 `TBL_HonChonMadoServer` 的文件 |
| `extinctionMongen.txt` | 8586 | 定时刷怪（`@tuesdayMonGen` 等） |
| `HelperHonChonDoBook.txt` | 7988 | 25 阶段任务日志读取器 |
| `HelperHonChonDoBook.txt.utf8` | 8000 | **重复文件，全部位于一行**；无引用，应忽略，否则会干扰搜索 |
| `HonMainControl.txt` | 4635 | 每周调度器 |
| `HonChondonpc.txt` | 4352 | 入口 NPC，交付寂幻之刃 |
| `MadoNpcHyun.txt` | 3012 | 玄 |
| `Map_extinctionRoom.txt` | 1976 | 地图脚本，10 处引用 |
| `Map_honchonmadoRoom.txt` | 1901 | 地图脚本 |

<a id="questdiaryhonchonmado-16-files"></a>
### `QuestDiary/HonChonMado`（16 个文件）

| 文件 | 字节数 | 内容 |
|---|---|---|
| `Mon_MadoHave.txt` | 18085 | 怪物钩子，46 处引用、40 次更新 |
| `MadoNpc.txt` | 10987 | 魔达；**创建 `TBL_MadoHaveUserQuest` 记录**（第 37 行） |
| `MonsterNpc1.txt` | 8758 | 27 处引用 |
| `QuestMadoEvent.txt` | 6029 | 事件衔接逻辑 |
| `StoneNpc3.txt` | 4783 | 守护石 3 |
| `MadoHaveJaha.txt` | 4678 | |
| `StoneNpc4.txt` | 4503 | 守护石 4 |
| `SahakaNpc.txt` | 3851 | 也会创建记录（第 28 行） |
| `NumaNpcMagda.txt` | 3463 | |
| `NumaNpcMagda1.txt` | 2776 | |
| `StoneNpc2.txt` | 2535 | |
| `StoneNpc1.txt` | 2406 | |
| `StoneNpc5.txt` | 2263 | |
| `HelperHonChonMaDoBook.txt` | 1955 | 11 阶段任务日志读取器 |
| `StoneNpcbox1.txt` | 1726 | |
| `ItemQuestMado.txt` | 957 | |

<a id="prose-lives-elsewhere"></a>
### 对话文本位于其他文件

每个 `#SAY` 都使用 `#INCLUDE [Convert_Def/QuestDiary/<dir>/<same-filename>] @Convert_<label>_n`。因此上面的文件各有保存实际文本的**配对文件**，位于 `Convert_Def/QuestDiary/HonChonDo/`（14 个，重复的 `.utf8` 文件没有配对）和 `Convert_Def/QuestDiary/HonChonMado/`（16 个，一一对应）。288 次 `LoadValue` 调用也以同样方式读取短字符串。转换时应像技能任务一样把文本内联到 `.lua` 中，不要复制这层间接引用。

<a id="outside-the-two-directories"></a>
### 两个目录之外的文件

- `QuestDiary/NpcQuest_Def/17baska1-npcroom1.txt` 至 `17baska4-npcroom1.txt`：候场区脚本，各有一处 `TBL_HonChonMadoQuest` 引用。
- `QuestDiary/NpcQuest_Def/17Zaha_Numa-41.txt`：一处引用。

<a id="11-legacy-data-quirks-to-expect"></a>
## 11. 原版数据中需要留意的问题

与 `Envir` 其他部分一样（参见 `legacy-data-is-buggy` 说明），可能存在不可达标签、自引用 goto、无法满足的检查和不一致的 NPC 名称。已发现的问题：

- `TBL_HonChonMadoSeverFlag`：原始数据本身拼写错误。必须保留该拼写，否则 SQL 无法匹配；不要悄悄“修正”。
- `HelperHonChonDoBook.txt.utf8`：`.txt` 的单行重复文件，大小接近，没有任何读取方；会干扰涉及该文件的 `grep -n`。
- HonChonDo 日志中**两个阶段都标为 1-3**，分别是获取宝箱和开启宝箱。
- 阶段 1-23 使用 `{FCOLOR/13}`，其他阶段均使用 `{FCOLOR/10}`。
- `HelperHonChonDoBook` 到达 `Large D0 13`，但日志只定义了 11 个 HonChonMado 阶段；计数器超出日志范围。
- 几个阶段描述在本应出现坐标的位置直接使用 `(???:???)` 占位符，还有一个 `ApplyMonEx ... "???"` 称号；原版本就没有填写。
- `FLD_MADOPATTERN` / `FLD_MADOTEMPQUEST` 只出现在插入列列表；围绕它们设计表结构前应确认是否有读取方。

<a id="12-suggested-order"></a>
## 12. 建议实施顺序

1. 实现脚本级 SQL，或为多级进度和共享状态选择其他持久化模型；这也能解除攻城战的相关限制。
2. 实现现实时间调度器：星期、小时、分钟及全局重复计时。
3. 实现 `Movenpc`，或决定放弃候场区，由 NPC 自行按时间控制响应。
4. 实现广播（`Eventmsg grobal`、`linemsg`）。
5. 补充 15 条物品记录、阿龙怪怪物记录；若地图数据存在，再添加 `D16061` / `D16062`。
6. 实现 `Mongenp` 掉落覆盖和 `ApplyMonEx` 阵营，再实现刷怪时间表。
7. 最后才开始转写对话；先完成 HonChonDo，因为寂幻之刃是进入 HonChonMado 的前提。

<a id="13-how-to-verify-a-conversion"></a>
## 13. 如何验证转换

为技能任务编写的工具可直接使用；调研时它们位于会话临时目录，依赖之前需要将其保存在持久位置：

- `questharness.lua <file>`：使用模拟全局对象运行任务插件，用 `load()` 加载每段内嵌代码字符串，遍历每个 FSM 状态和 NPC 处理器，渲染全部布局并检查 XML 标签配对。
- `npcharness.lua <file> <include-dir>`：对 NPC 脚本执行同类检查。**include 目录是必需的第二个参数**；缺少时 `require` 会静默返回 `{}`。
- `coverage.lua <legacy-src.txt> <converted.lua>`：在转换后文件中搜索每条原版对话并报告缺失。带格式替换（`%s`）的行可能被误报为缺失；`goto` / `MonGen` / `give` 命令行属于干扰项。

使用 `luac -p` 检查语法时，**先复制为 ASCII 临时文件名**：调研时的 Windows `luac` 无法打开中日韩字符路径。生成中日韩字符文件名后，还应扫描乱码文件名：

```
git ls-files -z | python -c "import sys;[print(repr(p)) for p in sys.stdin.buffer.read().decode().split(chr(0)) if any(0xE000<=ord(c)<=0xF8FF or ord(c)<0x20 for c in p)]"
```

该检查曾发现 11 个技能导师文件名尾部带有 U+F00D，导致服务端从未加载这些文件（`24a16db5`）。
