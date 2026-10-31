# 内容与 Lua 脚本

[返回知识库](README.md)

## 先判断改的是哪一层

| 修改对象 | 文件位置 | 使改动生效 |
| --- | --- | --- |
| 引擎行为、脚本绑定 | `server/src/*.cpp`、`*.hpp` | 重新编译、安装并重启 |
| 内嵌 Lua 框架 | `server/src/player.lua`、`quest.lua`、`npchar.lua`、`servermap.lua` 等 | 相关文件由 C++ `#embed` 引入，重新编译、安装并重启 |
| 内容脚本、远程 API | `server/script/` | 重新安装到 `install/server/script/`，重启相应加载流程 |
| 静态业务定义 | `common/src/*record.inc` | 编译受影响的客户端、服务端和工具，检查 ID 兼容性 |
| 图片、音频、地图二进制 | `mir2x_res` 独立资源仓库 | 重新打包受影响资源，再使用新资源验证 |

不要只编辑 `install/server/script/` 作为长期开发方式：下一次安装可能被源码覆盖。该目录可以用于定位问题，但最终修改应回到 `server/script/`。

这份文档没有承诺所有脚本都支持无重启热更新。是否可重载，应针对对象生命周期和已安装回调另行确认。

## 地图与刷怪

地图分三层：

1. [maprecord.inc](../../common/src/maprecord.inc) 给出地图业务定义，例如名称及相关属性。
2. `mir2x_res/map` 经打包生成 `mapbin.zsdb`，由 `MapBinDB` 读取地图单元数据。
3. [server/script/map](../../server/script/map) 给出地图运行脚本，例如刷怪。`ServerMap` 优先加载 `<地图记录名>.lua`，没有时尝试 `default.lua`。

新增一份地图 Lua 文件不会自动创造地图二进制或静态记录。反过来，只增加贴图也不等于添加可加载的地图。

刷怪参考 [北部石矿1层_D1411.lua](../../server/script/map/北部石矿1层_D1411.lua) 和 [addmonster.lua](../../server/script/map/addmonster.lua)：现有内容使用 `require('map.addmonster')`、`monGener()`，按名称和区域定义怪物，再在 `main()` 中恢复生成协程。

修改前先核对怪物名称能否通过 `DBCOM_MONSTERID` 对应到定义，区域是否在可通行地图内。坐标、数量、周期等字段的具体单位和调度语义，应以 `addmonster.lua` 的消费代码为准。

启动参数 `--preload-map`、`--disable-map-script`、`--disable-monster-spawn` 会影响观察结果。只打开服务器窗口还未 Launch 时，地图内容也尚未进入正常运行流程。

## NPC 与商人

[ServerMap::loadNPChar()](../../server/src/servermap.cpp) 按实际文件名正则识别 NPC：

```text
<地图名>.<NPC名>.GLOC_<x>_<y>_<dir>.LOOK_<lookID>.lua
```

这里的地图名要与 `MapRecord.name` 一致，NPC 名可包含内容自身的编号。坐标、朝向与外观 ID 由文件名提取，不只取决于脚本正文。注意该函数中的旧格式注释与正则的顺序不同，本文采用实际正则的顺序。

NPC 的运行入口在 [npchar.cpp](../../server/src/npchar.cpp)、[npchar.lua](../../server/src/npchar.lua)，客户端点击通过 `CM_NPCEVENT` 进入 [Player::net_CM_NPCEVENT()](../../server/src/playernet.cpp)，再转入 NPC 行为。

商人实现位于 [merchant](../../server/script/npc/include/merchant)：铁匠、服装商、首饰商、药商、书商等模块各自拥有菜单与业务。已有 NPC 脚本演示了 `require('npc.include.merchant.smith')` 等用法，优先从同类型 NPC 复制最小结构，再逐条核对原有行为。

要保持的边界：

- [invop.lua](../../server/script/npc/include/invop.lua) 提供背包交易、保管、修理等底层操作。
- 商人类型模块决定自身菜单、事件与服务，不把所有商人重新折成一个统一菜单生成器。
- 店内销售库存不能直接推导店家回收的全部类别。
- 普通修理与特殊修理是不同服务；耐久与收费也有独立规则。
- 转换原版脚本时保留来源、实际分支、对白和缺失内容说明，不能用另一种物品悄悄替换缺失的任务物品。

完整规则以 [商人说明](../../server/script/npc/include/merchant/README.md) 为准，其中也列有尚未实现或只能部分表达的原版能力。

## 远程调用的执行位置

[server/script/api](../../server/script/api) 的 `server.player`、`server.quest`、`server.npc` 等接口将脚本操作送到对应对象。例如 `server.player.addItem()` 最终通过 `uidRemoteCall()` 在玩家对象上下文执行。

`uidRemoteCall()` 发送的代码与参数需要按当前 runner 支持的形式序列化；函数闭包和带原生元表的对象不能假定可以传输。需要参数时显式传值，不依赖调用端的局部变量自动在远端出现。

关键文件：[serverluacoroutinerunner.cpp](../../server/src/serverluacoroutinerunner.cpp)、[serverluacoroutinerunner.lua](../../server/src/serverluacoroutinerunner.lua)、[serverobject.cpp](../../server/src/serverobject.cpp)、[player API](../../server/script/api/player.lua)、[quest API](../../server/script/api/quest.lua)。

对话关闭可能取消 NPC 协程，但已经送达的玩家操作仍可能执行。涉及材料交换时，把条件检查、扣除和奖励放在同一次合适的玩家侧操作中，返回可序列化的结果；不要让取消对话把“扣材料”和“发奖励”拆散。

## 任务状态与奖励

[ServiceCore::onActivate()](../../server/src/servicecore.cpp) 扫描 `script/quest` 的顶层 Lua 文件，为每个脚本创建 Quest。文件名参与任务名和持久化表名；不能随意重命名已有任务。

开发任务时，先阅读 [任务框架规则](../../server/script/quest/README.md)，再选相近任务作为参考。简单触发示例是 [升级奖励.lua](../../server/script/quest/升级奖励.lua)，复杂任务则使用主/子 FSM。

必须理解的行为：

1. `setQuestFSMTable()` 声明状态表，`setQuestState()` 切换玩家的任务状态。
2. 玩家登录会从头重放当前状态函数。不能把不可重复的扣物、扣款、发奖随意放进重放函数。
3. NPC 回调、定时器、格子触发和远程返回可能已经过时，状态切换要带 `from=` 条件。
4. 状态运行线程切换自己的状态后不会继续返回；写在切换之后的同线程代码不能当成必定执行。
5. 回调发奖应先确认相应状态切换成功，再发奖；需要先扣款的场景，要明确切换失败时的返还逻辑。
6. 副本 UID、线程 key 是临时值，应使用运行时变量，不能当成可跨重启的永久标记。
7. 在途远程操作、重复点击、重新登录和完成后的旧回调，都应纳入任务验收。

任务“状态切换成功后只发一次奖励”的模式可参考：

```lua
-- 在 NPC 等外部回调中；questUID/playerUID 来自当前业务上下文。
if not server.quest.setState(questUID, {
    uid = playerUID,
    from = 'quest_trial_passed',
    state = SYS_DONE,
}) then
    return
end

server.player.addItem(playerUID, '木剑', 1)
```

示例只说明重复回调的状态门槛，不提供“数据库提交与奖励跨 Actor 崩溃时也恰好一次”的保证。需要这种保证的业务还要设计持久化奖励记录或恢复机制。

## 资源的构建与验证

资源仓库是 `https://github.com/etorth/mir2x_res.git`；默认路径由 [Mir2xResPath.cmake](../../cmake/Mir2xResPath.cmake) 管理，也可通过 `build.py --res-path=...` 指定已有资源。

[install_res.py](../../tools/installres/src/install_res.py) 定义 17 个客户端资源包和 1 个服务端地图包。它比较输入树和输出文件的修改时间，最新时跳过；缺失或过期时调用 [zsdbmaker](../../tools/zsdbmaker/src/main.cpp)。

- `.zsdb` 是自定义资源容器，不是 SQLite 存档。
- `ZSDB::buildDB()` 读取指定目录中的非空普通文件，再构造索引和数据；不要假定会自动递归打包任意层级子目录。
- 图片文件、贴图 ID、偏移信息及 `ItemRecord` 等业务定义需要配套核对。替换 PNG 不一定能完成外观功能。
- 安装调用包含 `--ignore-zsdbmaker-error`；因此安装退出成功不等于每个资源包都已打包成功。要检查警告、实际文件及容器可读性。

在 UCRT64 终端、仓库根目录中，检查一个包能否打开：

```bash
build/tools/zsdbmaker/src/zsdbmaker.exe \
    --list=install/client/res/texture/proguse.zsdb
```

输出会列举条目；大包可能输出很多行。条目数应与本次实际输入的非空文件对应，不能硬编码成所有未来版本都固定的数量。

首次资源克隆和打包很慢，尤其在 Windows 上处理几十万个小文件。保留缓存和已经生成的包。当前工程的标准安装仍是逐包处理；本次使用的 `.vcpkg/pack-resources.py` 是本机辅助工具，不是已合入的正式构建接口。

## 内容开发的验收维度

| 内容 | 至少验证 |
| --- | --- |
| 地图/刷怪 | 能加载、可通行区域、数量与周期、重启后的行为 |
| NPC | 文件名识别、位置/外观、菜单入口、返回与关闭、资格不满足时的行为 |
| 商店/兑换 | 价格、余额不足、材料不足、重复提交、取消对话、扣除和获得一致 |
| 任务 | 首次进入、拒绝/重复点击、切换状态、重登重放、完成后旧回调、重启恢复 |
| 道具/技能 | 静态 ID、客户端呈现、服务端效果、保存和重登、已有实例兼容 |

脚本测试见 [开发与测试流程](workflow-and-testing.md#现有测试地图)。测试使用模拟世界的场景，不取代真实客户端中的操作验证。
