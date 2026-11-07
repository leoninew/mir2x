# Lua 调试脚本

本文单独收录客户端 Lua 与服务端管理脚本。聊天窗口的 `@` 命令见 [GM 聊天命令](gm-commands.md)。客户端 Lua 在聊天框以 `$` 开头；服务端脚本只能在服务端命令窗口按指定模式执行。

## 客户端 `$` Lua

`$` 后面是 Lua 代码，不再使用 `@` 的空格参数规则。主要相关函数如下：

| 函数                                                                  | 行为                                                                               |
| --------------------------------------------------------------------- | ---------------------------------------------------------------------------------- |
| `moveTo()`、`moveTo(x, y)`、`moveTo(mapID)`、`moveTo(mapID, x, y)`    | 与 `@moveTo` 共用同一 Lua 绑定                                                     |
| `makeItem(itemID或物品名[, count])`                                   | 默认数量 1；物品名用 Lua 字符串，数字则按物品 ID 处理                              |
| `addExp(exp)`                                                         | 发送增加经验请求                                                                   |
| `getMapID("地图名")`、`getItemID("物品名")`、`getMonsterID("怪物名")` | 查询记录 ID；使用源码中的准确名称                                                  |
| `addCBLog(CBLOG_SYS, "文字")`                                         | 输出到客户端聊天日志                                                               |
| `playerList()`                                                        | 返回客户端当前可见玩家集合中的 UID 列表，不是全服在线玩家列表                      |
| `myHero_dress(itemID)`、`myHero_weapon(itemID)`                       | 直接修改本地 Hero 装备显示数据；没有服务器穿戴或物品授予流程，不能当成真正获得装备 |

示例：

```lua
$addCBLog(CBLOG_SYS, tostring(getMapID("道馆_1")))
$makeItem("木剑", 1)
$moveTo(getMapID("道馆_1"), 405, 120)
```

`$help(...)` 使用另一张旧帮助表，里面的 `wear`、`randMove` 等文字不能作为命令已实现的证据。查询 `@` 命令清单应使用 `@help`。

## 服务端 Lua 命令窗口

从服务端图形窗口打开 `Command → Run`，窗口标题为 `LuaCommand`。此处直接输入 Lua，**不加聊天栏的 `$` 前缀**。Enter 执行，Shift+Enter 换行，上下键浏览输入历史；执行期间输入框暂时禁用。

执行模式影响函数可见性，不能混用：

| 模式           | 实际行为                                                                        |
| -------------- | ------------------------------------------------------------------------------- |
| `LOCAL`        | 在命令窗口自身的 `CommandLuaModule` 执行；下表函数属于这个环境                  |
| `ASYNC`        | 要求 actor pool 已启动，经 `asyncEval` 转发到 `ServerLuaObject` 的协程 Lua 环境 |
| `AUTO`（默认） | actor pool 已启动时选 ASYNC，否则选 LOCAL                                       |

**使用下面的管理函数时，显式选择 LOCAL。** 服务启动后默认 AUTO 会切到另一套 Lua 环境，不能假定其中也有命令窗口的 `listMap()`、`addMonster()` 等函数。涉及地图、怪物的操作仍需要服务器相关 actor 已运行。

| LOCAL 函数                                      | 用途与返回值                                                                           |
| ----------------------------------------------- | -------------------------------------------------------------------------------------- |
| `listMap()`                                     | 在命令窗口列出服务器当前地图列表的 ID 和名称                                           |
| `getMapIDList()`                                | 返回服务器地图 ID 列表；不是全部静态地图记录清单                                       |
| `loadMap("地图名")` 或 `loadMap(mapID)`         | 加载基础地图，返回成功与否                                                             |
| `countMonster(monsterID, mapID)`                | 查询指定基础地图的指定怪物数量，失败返回 -1                                            |
| `addMonster(monsterID, mapID)`                  | 创建一只怪物，未指定坐标，返回成功与否                                                 |
| `addMonster(monsterID, mapID, x, y)`            | 在指定坐标请求创建一只怪物，默认非严格落点                                             |
| `addMonster(monsterID, mapID, x, y, strictLoc)` | 显式控制严格落点；最后一个参数实际传入 `strictLoc`，旧用法提示中的 `Random` 命名不准确 |
| `addCWLogString(0, "> ", "文字")`               | 在当前命令窗口输出一行                                                                 |
| `history()`                                     | 输出当前窗口的历史输入                                                                 |
| `quit()`                                        | 关闭当前命令窗口，不是关闭服务器                                                       |

LOCAL 查询示例：

```lua
listMap()
addCWLogString(0, "> ", tostring(getMapID("道馆_1")))
```

生成怪物使用 `addMonster(getMonsterID("准确怪物名"), getMapID("准确地图名"), x, y, true)`，其中名称与坐标须换成有效值。这会立即改变世界状态，不是修改刷怪配置，也不会形成新的周期刷怪规则。

服务端 Lua 还暴露数据库访问等开发能力；它不是一套仅限安全查询的 GM 指令白名单。经验倍率、掉落基数、刷怪配置等长期规则应在相应配置中管理，不能用临时加经验、造物、生成怪物代替。

### 给自己的角色加金币

当前没有 `@addGold` 或 `@dropGold` 聊天命令。金币余额独立存储，不占背包格。服务端 ASYNC Lua 可以调用目标在线玩家的 `addItem()`；该函数收到金币物品 ID 时会增加角色金币字段：

```lua
-- 将 123 替换成自己角色的 DBID
local playerUID = (UID_PLY << 59) | 123
uidRemoteCall(playerUID, [[
    addItem(getItemID("金币（小）"), 10000)
]])
```

目标必须在线。UID 由 `uidf::getPlayerUID(dbid)` 按 `(UID_PLY << 59) | dbid` 构造；示例中 `123` 是角色 DBID，不是账号名，也不是已构造的 UID。推荐用此路径直接增加余额，不要用 `@makeItem` 或客户端 `makeItem` 充当加钱命令。

若要让金币掉在地上，服务端 ASYNC Lua 可给一只仍活动、允许正常死亡掉落的怪物设置一次死亡掉落：

```lua
setMonsterDropOnDie(MONSTER_UID, {
    { item = { itemID = "金币（小）", count = 10000 }, odds = 1 },
}, { defaultDrop = true })
```

将 `MONSTER_UID` 换成实际怪物 UID；怪物正常死亡后金币才进入地面掉落。`defaultDrop = true` 保留该怪物原有默认掉落。这不是立即在指定坐标造金币，也不修改长期刷怪或掉落配置。以怪物死亡后生成的钱物数量为准，不把示例的调用受理当作地面结果已发生。

## 源码索引

- [聊天栏入口](../../client/src/gui/controlboard/controlboard.cpp)：`ControlBoard::onInputDone()` 中的 `@`、`$` 分发。
- [客户端命令、Lua 绑定与请求](../../client/src/processrun.cpp)：`userCommand()`、`RegisterUserCommand()`、`registerLuaExport()`。
- [GM 身份定义](../../common/src/uidf.cpp)：`uidf::isGM()`。
- [公共 Lua 名称与 ID 查询](../../common/src/luamodule.cpp)：`getItemID/getMonsterID/getMapID` 等绑定。
- [玩家协议分发](../../server/src/player.cpp)：`Player::operateNet()`。
- [服务端命令效果](../../server/src/playernet.cpp)：`net_CM_REQUESTSPACEMOVE/REQUESTADDHEALTH/REQUESTADDEXP/REQUESTDIE/REQUESTKILLPETS/MAKEITEM`。
- [等级上限与累计经验](../../common/src/sysconst.hpp)：`SYS_MAXLEVEL`、`SYS_SUMEXP`。
- [服务端菜单](../../server/src/mainwindow.fl)、[命令窗口](../../server/src/commandwindow.fl)、[输入和执行模式](../../server/src/commandinput.cpp)。
- [LOCAL 管理函数](../../server/src/server.cpp)：`Server::regLuaExport()`；[窗口 Lua 辅助函数](../../server/src/server.lua)。
- [异步执行转发](../../server/src/commandluamodule.cpp)、[服务端协程对象](../../server/src/serverluaobject.cpp)、[基础服务端 Lua](../../server/src/serverluamodule.cpp)。
- [玩家金币与物品 Lua 绑定](../../server/src/player.cpp)：`addItem()` 将金币物品转换为余额增加；[UID 构造](../../common/src/uidf.cpp)；[怪物死亡掉落设置](../../server/src/serverluacoroutinerunner.lua) 与[死亡结算](../../server/src/monster.cpp)。
