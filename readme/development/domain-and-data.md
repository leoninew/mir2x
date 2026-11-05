# 业务模型与数据

[返回知识库](README.md)

## 账号、角色与身份

| 概念 | 当前表示 | 不能混用的对象 |
| --- | --- | --- |
| 账号 | `tbl_account`：`fld_dbid`、账号、密码 | 账号名不等于角色名 |
| 角色 | `tbl_char`，主键也是 `fld_dbid`，引用账号 | 当前不是“一账号多角色槽”模型 |
| 数据库身份 | `dbid`，通常是 32 位整型 | 不是网络连接 ID，也不是完整 UID |
| 玩家运行时身份 | `uidf::getPlayerUID(dbid)`，64 位 UID | 不要把 dbid 直接传给要求 UID 的 API |
| 连接身份 | `channID`，连接建立、绑定和回收时使用 | 断线重连后不应作为存档主键复用 |
| 地图类型 | `mapID` 对应静态 `MapRecord` | 不等于某一个运行中的地图实例 |
| 地图实例 | `mapUID`，可表示基础地图或副本 | 同一种地图可以有不同实例 |

依据：[server.sql](../../server/src/server.sql)、[uidf.hpp](../../common/src/uidf.hpp)、[uidf.cpp](../../common/src/uidf.cpp)、[servicecore.hpp](../../server/src/servicecore.hpp)。

`tbl_char.fld_dbid` 是主键，因此同一账号最多一条角色记录。要支持多角色，需要同时调整表主键、Player UID、查询/删除/上线协议和多处按 dbid 存储的业务，不能只扩充选角 UI。

UID 包含对象类型和相应的身份信息，部分类型包含节点信息。最高位不使用，以便 Lua 有符号整数表示。分类应调用 `uidf::isPlayer()`、`isMap()`、`getPlayerDBID()` 等函数，不在业务代码里自行拆位。Quest UID、Player UID、NPC UID 和 Monster UID 不互换。

当前 `uidf::isGM()` 按玩家 `dbid <= 10` 判断 GM，不是读取独立的权限表。开发权限相关功能时，应核对这个实际判断及其调用方，不能只凭角色名称或界面标识推断权限。

## 职业、属性和成长

- 职业位在 [protocoldef.hpp](../../common/src/protocoldef.hpp) 中定义：战士 `1 << 0`、道士 `1 << 1`、法师 `1 << 2`。
- [jobf::jobValid()](../../common/src/jobf.hpp) 允许合法非零组合，默认测试角色就包含多职业。职业判断应使用位判断或既有 `hasJob()`，不能一律写 `job == JOB_WARRIOR`。
- `tbl_char` 保存经验、HP/MP、金币、出生/离线地图坐标、外观、PK 点和视距等；等级及战斗派生属性还需要结合 `Player`、`BattleObject` 和相应静态记录计算。
- 常见属性缩写为 DC（物理攻击）、MC（魔法攻击）、SC（道术）、AC（物理防御）、MAC（魔法防御）。具体计算、上下限和修正要查实现，不能只按缩写推导公式。
- 学习技能列表位于 `tbl_learnedmagiclist`；技能定义是 `magicrecord.inc`，不是同一份数据。

入口：[player.cpp](../../server/src/player.cpp)、[battleobject.cpp](../../server/src/battleobject.cpp)、[magicrecord.inc](../../common/src/magicrecord.inc)、[bufflist.cpp](../../server/src/bufflist.cpp)。

## 静态定义与实例

| 数据 | 静态定义 | 运行/持久化实例 |
| --- | --- | --- |
| 物品 | `ItemRecord`、`itemrecord.inc` | `SDItem`、背包/装备/交付/拍卖中的物品 |
| 怪物 | `MonsterRecord`、`monsterrecord.inc` | `Monster` Actor |
| 地图 | `MapRecord`、`maprecord.inc` | `ServerMap`、基础地图 UID 和副本 UID |
| 技能 | `MagicRecord`、`magicrecord.inc` | 玩家已学技能、施法及效果 |
| Buff | `buffrecord.inc`、`buffactrecord.inc` 等 | `Buff`、`BuffAct` 与对象上的活动效果 |

[dbcomid.hpp](../../common/src/dbcomid.hpp) 把 `.inc` 记录编成数组，并建立名称查找，例如 `DBCOM_ITEMID(u8"木剑")`。ID 与数组位置相关，不是可任意重新排序的描述文本。

新增定义优先考虑追加；在已有定义之间插入、删除或重排之前，应检查存档和协议中的数字 ID，并设计旧 ID 到新 ID 的映射。也要核对脚本中的中文名称；改名会影响名称查找和脚本行为。

## 道具实例与容器

[SDItem](../../common/src/sditem.hpp) 的核心字段是：

| 字段 | 含义 |
| --- | --- |
| `itemID` | 哪一种静态物品 |
| `seqID` | 容器业务中区分同类实例的序号；不是全服唯一 GUID |
| `count` | 数量；不能把所有物品都当成单件装备 |
| `duration[0]`、`duration[1]` | 当前、最大耐久；最大耐久也是实例属性 |
| `extAttrList` | 序列化扩展属性，例如攻击修正、品质、绑定等 |

`tbl_inventory` 和 `tbl_secureditemlist` 主键都是 `(fld_dbid, fld_itemid, fld_seqid)`。操作物品时要保留实例身份，不要仅凭 itemID 删除一件同类装备。

容器并非同构：背包、保管列表保存 seqID；装备由 `fld_wear` 槽位定位；腰带由 `fld_belt` 槽位定位。转移时应使用已有业务操作，不能简单把一张表的主键字段复制成另一张表的字段。

`EA_*` 扩展属性由宏按声明顺序生成数字编号。中间插入新属性同样可能改变已保存的编号；兼容性检查不能只盯住物品记录 ID。

## 经济与持久化

| 业务 | 主要入口 | 要关注的不变量 |
| --- | --- | --- |
| 金币 | [golddb.cpp](../../server/src/golddb.cpp) | 金额范围、足额扣款、内存与数据库同步 |
| 背包 | [inventorydb.cpp](../../server/src/inventorydb.cpp)、`Player` 物品操作 | 实例身份、数量、容量和扩展属性 |
| 商店买卖、修理 | [playernet.cpp](../../server/src/playernet.cpp)、[NPC invop](../../server/script/npc/include/invop.lua) | 重新校验报价和物品；扣款、扣物与收益的一致性 |
| 玩家直接交易 | [directtradedb.cpp](../../server/src/directtradedb.cpp) | 双方锁定并确认、足额、双方物品和金币一起提交 |
| 拍卖 | [auctiondb.cpp](../../server/src/auctiondb.cpp)、`Player::net_CM_*AUCTION*` | 卖家、价格、过期、下架和物品/金币归属 |
| 交付物品 | [deliverydb.cpp](../../server/src/deliverydb.cpp)、`net_CM_CLAIMDELIVERY()` | 未领取与已领取状态、重复领取 |
| 聊天和关系 | [chatdb.cpp](../../server/src/chatdb.cpp) | 玩家、群组、好友、黑名单与消息身份 |

`dbRemoveGold()` 使用带余额条件的 UPDATE 并返回余额；`dbCommitDirectTrade()` 有显式事务。这些是应沿用的实现入口，不代表所有经济功能都已经有一致的事务边界。

[DBPod](../../server/src/dbpod.hpp) 提供 `createQuery()`、`exec()`、`createTransaction()` 和 BLOB 参数绑定。阅读多步操作时要继续查调用方是否创建事务；例如“清空后逐条重写”的底层函数本身不自动等于原子保存。

跨 Actor 的远程调用可能等待，也可能在对话取消后继续完成。新增交换功能时，应把检查、扣除和发放放在合适的同一业务操作中，并明确失败补偿；详见 [脚本规则](content-and-scripting.md)。

## 任务持久化

每个 Quest Actor 对应一个任务脚本。[Quest::getQuestDBName()](../../server/src/quest.hpp) 根据任务名称构造表名；[questdb.cpp](../../server/src/questdb.cpp) 按需创建表。

一条任务记录以玩家 dbid 为主键，保存 `fld_states`、`fld_flags`、`fld_team`、`fld_vars`、`fld_desp`、`fld_context` 等序列化字段。任务脚本改名可能改变表名，应视为存档兼容性变化。

主/子 FSM 状态、完成标记、状态附带的世界配置，与临时运行时变量不是同一层数据。副本 UID、线程 key 等重启后无效的值不能直接当成永久任务标记保存。具体重放规则以 [任务框架说明](../../server/script/quest/README.md) 为准。

## 结构变更与存档兼容

当前 `Server::createDBConnection()` 在检测不到业务表时执行初始化 SQL；现有表存在时不会因此自动重建整个 schema。这不是完整的版本化迁移系统。

新增字段或变更含义时至少确定：

1. 新数据库的初始化结构和默认值。
2. 现有开发存档怎样升级，是否需要回填、映射或保留旧值。
3. C++ 查询、BLOB 序列化、Lua 字段以及网络结构是否同时变化。
4. 失败时怎样保持原数据可读；不要用删除玩家数据库代替迁移说明。

`server.sql` 使用 STRICT 表、外键及由服务端注册的 `jobValid()`、`SYS_PLY_VIEWR()`、`checkAuctionCategory()` 等 SQL 函数。直接在普通 SQLite 命令行执行全部初始化 SQL，可能因为这些函数未注册而失败。

`tbl_account.fld_password` 当前存储密码文本，登录代码也有直接拼接查询的实现。它描述的是实验项目现状；面向公网和真实用户之前，需要单独设计身份认证、输入与权限边界。默认账号和现有调试请求不能直接当成运营规则。
