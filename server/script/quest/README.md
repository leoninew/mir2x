<a id="quest-scripts"></a>
# 任务脚本

每个任务都是由原版资料转换而来的独立脚本。脚本应保留原始流程、分支和文本，不要把任务合并成共用模板。框架位于 `server/src/quest.lua`，远程 API 位于 `server/script/api/quest.lua`，怪物掉落逻辑位于 `include/mondrop.lua`。

<a id="states-and-state-runners"></a>
## 状态与状态运行线程

`setQuestFSMTable{...}` 声明主有限状态机（FSM），`setQuestFSMTable(name, {...})` 声明子 FSM。`setQuestState{uid=uid, state='x'}` 写入状态，并在新线程中运行该状态函数；这个线程是对应 `{uid, fsm}` 的状态运行线程：

- 状态切换会立即关闭旧的状态运行线程，即使它正处于 `pause()`、`uidRemoteCall()` 或 `waitNotify()` 中；它正在等待的回复会被丢弃。
- 状态运行线程切换自己的状态时，调用不会返回。状态函数及其调用者中位于该调用之后的代码都不会继续运行。
- 其他线程发起的状态切换成功时返回 true；如果被 `from=` 拒绝则返回 false。
- 主 FSM 切换到 `state=SYS_DONE` 时，任务完成：关闭该玩家所有 FSM 的状态运行线程并清除其状态，数据库记录只保留完成状态。
- 之后 FSM 表中的 `[SYS_DONE]` 函数运行一次，不会重放。可在这里调用 `setQuestDesp()` 设置登录时显示的结束描述。

<a id="a-login-replays-the-current-states"></a>
## 登录时重放当前状态

每次登录都会从头重新运行各 FSM 当前状态的状态函数。因此，状态函数不应发放奖励，也不应扣除不能重复扣除的内容；这些操作应放在切换到该状态的回调中，并按下文规则处理切换与发奖顺序。

重放之前，状态安装但尚未提交的世界变更项会再次被移除。所有状态重放完毕后触发 `SYS_ON_ONLINE`；试炼会为 `SYS_ON_ONLINE`、`SYS_ON_OFFLINE` 和 `SYS_ON_DIE` 注册放弃函数，因此试炼不会在登出或重启后继续保留。

<a id="world-items-are-saved-with-the-state"></a>
## 世界变更项随状态保存

以下函数将世界变更安装为任务上下文中的项。这些项在结束安装它们的状态时随状态切换保存，并在重启后的首次登录时重新安装：

```lua
setupNPCQuestBehavior(mapName, npcName, uid, argstr, code)
clearNPCQuestBehavior(mapName, npcName, uid)

setupMapUIDGridTrigger{uid=uid, name='door', map=mapName, x=x, y=y, argstr=argstr, code=code}
clearMapUIDGridTrigger{uid=uid, name='door'}
```

- `argstr` 在每次安装时重新执行，不能捕获脚本中的变量；应使用 `string.format()` 和 `asInitString()` 传入值。
- 格子触发器有名称；再次安装同名触发器会替换原触发器，在另一张地图安装时则会将它移到新地图。处理函数恰好返回 true 时触发器失效。
- 任务完成时会删除所有已保存的项；向已完成的任务安装项会抛出错误。
- `setupInstanceNPCBehavior()` 和 `setupInstanceUIDGridTrigger{}` 作用于地图副本，不会保存，因为副本无法跨重启保留。
- `setupMapGridTrigger()` 从脚本加载时开始安装面向所有玩家的触发器，不会保存。

<a id="callbacks-switch-with-from"></a>
## 回调切换状态时使用 `from=`

NPC 对话会一直保留到被替换，通常会跨越多个状态；格子触发器、玩家触发器、定时器和怪物掉落会一直保留到触发。在所属 FSM 的状态运行线程之外发起切换时，都应通过 `from=` 指明安装该回调的状态，或它预期保持有效的状态集合：

```lua
server.quest.setState(questUID, {uid=uid, from='quest_find_scholar', state='quest_find_guard'})
setQuestState{uid=playerUID, from={'quest_give_soju', 'quest_wait_guards'}, state='quest_wait_guards'}
```

开始任务时使用 `from = SYS_LUANIL`，表示尚无状态，因此再次接受不会改变状态。对于已完成的任务，此处每个 FSM 都视为 `SYS_DONE`。如果 `from` 指定了 FSM 中不存在的状态，会抛出错误；拼写错误会使切换始终无法成功。在远程调用等会让出执行权的操作之前检查过的状态也应放入 `from=`，因为等待期间状态可能已变化。

<a id="rewards-come-after-their-switch"></a>
## 状态切换成功后发奖

状态切换在 Quest Actor 内执行比较并设置（compare-and-set）。先执行切换，只有成功时才发奖：第一次点击的远程调用仍在执行时发生的第二次点击，或先前状态留下的对话，都不会再次获得奖励。

```lua
npc_take_book = function(uid, value)
    if not server.quest.setState(questUID, {uid=uid, from='quest_trial_passed', state=SYS_DONE}) then
        return
    end

    server.player.addItem(uid, '...', 1)
end,
```

任务需要扣除的金币或物品应在切换前扣除；如果切换被拒绝，则应返还。如果等切换后才扣除，金币或物品可能已被花费，导致状态切换成功却没有完成付款。

```lua
npc_buy = function(uid, value)
    if not server.player.removeGold(uid, 3000) then
        return
    end

    if not server.quest.setState(questUID, {uid=uid, from='quest_buy', state='quest_bought'}) then
        server.player.addItem(uid, SYS_GOLDNAME, 3000)
        return
    end

    server.player.addItem(uid, '童子像', 1)
end,
```

向 Quest 发送执行代码时，应返回结果：`return setQuestState{...}`。如果切换后运行的状态函数要读取回调写入的内容，例如任务标记，应先写入该内容。没有状态切换的奖励应使用任务标记，并在一次对 Quest 的远程调用中检查和设置，中间不能让出执行权：

```lua
if uidRemoteCall(questUID, uid,
[=[
    local playerUID = ...
    if hasQuestFlag(playerUID, 'gift') then
        return false
    end

    addQuestFlag(playerUID, 'gift')
    return true
]=]) then
    server.player.addItem(uid, '...', 1)
end
```

<a id="monster-drops"></a>
## 怪物掉落

在掉落所属状态内部调用 `mondrop.addDropTrigger(uid, dropList)`，该掉落只在此状态有效：离开状态后再击杀会移除掉落且不发放物品；带 `setState` 的掉落先从该状态切换，再执行扣除和发放。玩家登出时会移除其掉落；下次登录重放状态时会重新添加。

<a id="values-that-dont-survive-a-restart"></a>
## 无法跨重启保留的值

地图副本 UID 和线程 key 会在重启后重新生成。应使用 `setQuestRuntimeVar(uid, key, value)` 保存这些值，而不是使用 `dbSetQuestVar()`；运行时变量不会持久化，并在任务完成时清除。

<a id="timers"></a>
## 定时器

`runQuestThread(func)` 在独立线程运行函数并返回线程 key；其中的 `pause()` 会延迟后续执行，`closeThread(key)` 用于取消线程。切换任务状态的定时器和其他回调一样需要使用 `from=`。状态结束时应取消其定时器，例如在状态函数中使用 `guardScope()`，或由后续状态取消。

对状态运行线程调用 `closeThread()` 会抛出错误；应使用 `setQuestState()` 结束状态。

<a id="errors"></a>
## 错误处理

状态函数抛错时，会回滚它安装但尚未提交的项并记录错误；任务保持当前状态，下次登录时重放。如需更早恢复：

- 在 FSM 表中使用 `stateWithFallback(func, fallback)`：每次运行该状态都会执行 fallback，包括重放；推荐使用。
- 使用 `setQuestState{..., fallback=function(uid, args, err) ... end}`：fallback 只适用于本次切换启动的运行。
- 使用 `setQuestState{..., fallback=[[ local uid, args, err = ... ]]}`：fallback 随状态保存，重放时也会执行，且可由 `server.quest.setState()` 发送。

fallback 再次进入同一状态时，应先调用 `pause()`。

<a id="refused"></a>
## 会被拒绝的操作

以下操作都会在产生变更之前抛出错误：

- 某玩家的另一次状态切换正在执行时，再为同一玩家发起切换，例如在正在关闭的状态运行线程的 `<close>` 处理器中切换。
- 状态运行线程通过自己创建的协程切换自身状态，或在无法让出执行权的位置切换，例如 `table.sort()` 的比较函数。
- 指定未知状态、FSM 不存在的 `from` 状态、为没有状态函数的状态设置 fallback，或通过远程调用发送函数形式的 fallback。
- 状态之间循环切换且没有让出执行权，达到 64 层。
- 对状态运行线程调用 `closeThread()`。

`<close>` 处理器在关闭过程中作为普通 C 代码执行，不能在其中调用 `pause()`、发起远程调用或切换状态；`guardScope()` 用于不让出执行权的清理操作。

<a id="tests"></a>
## 测试

`server/test/unit` 中的单元测试（`queststate`、`playerquest`、`gridtrigger`、`questdb`、`luarunner`）覆盖任务框架，可通过 `build.py --run-test` 运行。`questscripts` 套件还会使用模拟世界加载真实任务脚本，并通过内嵌 Lua 库运行商人和 NPC 回归测试。完成构建配置后，可在仓库根目录执行：

```sh
cmake --build build --target test_questscripts --parallel 8
ctest --test-dir build -R '^questscripts/' --output-on-failure
```

这些测试覆盖模拟环境实际执行的路径，并不覆盖每份脚本或每种游戏内交互。`luac -p` 只检查脚本本身，不检查其中在发送时才编译的代码字符串。框架测试套件、已安装脚本要求和人工验证方式参见[开发与测试指南](../../../readme/development/workflow-and-testing.md)。
