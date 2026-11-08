# Plan / 计划：持续攻击
最后修改时间: 2026-11-08 10:14:44

## Review status / 审查状态

Accepted

## Intent basis / 意图依据

- [持续攻击意图](../intent/20261108-continuous-attack.md)，A01–A06。
- 工作基线：`389fca6`，开始时工作区干净。

## Technical decisions / 技术设计与决策

- `SDRuntimeConfig` 按宏声明的数字序号序列化键。原位置将 `RTCFG_强制攻击` 改名为 `RTCFG_持续攻击`，不改变类型、默认值及后续配置索引。
- 复用 `FOCUS_ATTACK` 作为显式攻击目标，不新增独立的寻怪或定时攻击状态。`MyHero::emplaceAction` 统一同步攻击目标，非攻击操作解除锁定。
- `Hero::parseAction(ACTION_HITTED)` 清除正常动作队列，丢失原攻击模式动作上的续攻回调。`MyHero::moveNextMotion` 在受击动作最后一帧结束、强制动作和正常动作队列均空时检查配置与怪物目标，然后复用 `trackAttack` 和 `parseActionQueue`。
- 不在每帧触发器中直接排入恢复动作，避免 `Hero::update` 看到待处理动作后提前跳过受击帧。
- `trackAttack(false, uid)` 必须匹配当前攻击目标，阻止旧攻击回调覆盖新操作；同时校验双方存活。目标无效时在更新循环解除锁定，不保留等待其重新出现的攻击意图。
- 非攻击本地操作、NPC 对话、动作重置及角色站立纠正、死亡、传送清除攻击目标；追踪攻击拆分出的移动保持目标。网络受击清空待处理动作时保持目标，以供动作结束后恢复。
- 攻击目标被取消或切换时，由 `setFocusUID(FOCUS_ATTACK, ...)` 同时清除待执行的旧动作，避免旧追击队列在对话、跟随或死亡通知后继续执行。
- 复选框和快捷键共用 `RuntimeConfigBoard` 的设置入口，经现有 `CM_SETRUNTIMECONFIG` 和服务端保存流程持久化。快捷键只响应非重复 Shift+Tab，系统聊天显示开启/关闭状态。

## Implementation steps / 实施步骤

1. 原位重命名配置，统一复选框和快捷键设置入口（A01、A05）。
2. 增加受击结束后的怪物追踪恢复，并保护动作队列优先级（A02、A03）。
3. 贯通主动操作、目标失效和角色生命周期的解除锁定，防止旧回调重新接管（A04）。
4. 增加配置兼容性测试及不连接服务器的 SDL dummy 战斗回归测试，构建客户端及相关测试，审查最终 diff，记录开发检查和实机限制（A01–A06）。

## Files to change / 修改范围

- `common/src/sdruntimeconfig.hpp`：原位重命名配置。
- `client/src/gui/runtimeconfigboard/runtimeconfigboard.{hpp,cpp}`：设置文案和共享修改入口。
- `client/src/processrun.cpp`：快捷键、目标有效性及追踪保护。
- `client/src/processrunnet.cpp`：目标死亡、消失和离线时立即解除锁定。
- `client/src/myhero.{hpp,cpp}`：动作完成时恢复和生命周期取消。
- `common/test/unit/`：配置索引、默认值及旧布尔值兼容性回归测试。
- `client/test/unit/`：复用真实客户端对象和资源，在离线 SDL dummy 环境检查受击帧、恢复、取消和快捷键，不启动网络连接。
- 本功能的意图、计划及验证记录。

## Tasks and coordination / 子任务与协调

不适用。本会话实施，接收、进展和交付写在本计划的 Implementation delivery 节，不建立 task。

## Verification plan / 检查与验证安排

- A01：使用 Catch2 检查旧数字键、默认 true、保存 false、序列化还原及重置默认值。
- A02–A04：用真实 `ProcessRun` / `MyHero` 离线场景注入动作，检查受击帧结束才恢复、关闭不恢复、移动和目标失效取消、旧回调不能覆盖操作；同时审查生命周期路径。真实服务器对打与网络时序仍需实机操作，未运行不得记为通过。
- A05：检查 SDL 修饰键、重复事件抑制和配置共用入口，构建客户端并回归已有输入测试。
- A06：使用现有 CMake/CTest 入口；UCRT64 编译器和 Ninja 沿用当前 build 配置，使用 SDL dummy 驱动执行输入测试。必要开发检查由本会话执行，不连接或操控真实游戏角色。

## Risks and recovery / 风险与恢复

- 受击动画和连续服务端动作存在时序风险，必须同时检查帧结束与空队列，不在动画起始回调恢复。
- 不改变原有普通攻击链；关闭设置后仅取消新增的受击恢复行为。
- 修改均位于工作区，不自动暂存、提交或启动游戏。出现问题可以关闭“持续攻击”，或回退本次客户端修改；配置索引不需迁移。

## User review notes / 用户审查记录

- 用户“采纳 … 记录后实现”提供连续推进授权；按仓库事实完成方案，无待决设计问题。
- 2026-11-08：补充离线战斗回归方案，资源加载与网络启动在现有客户端中分离，测试只构造对象并缓冲消息，不调用连接或游戏主循环；不扩大产品功能范围。

## Implementation delivery / 实现交付

- 实现完成，交付为基于 `389fca6` 的本功能工作区变更，不包含 Git 暂存或提交。
- 原配置在位置 23 原位改名，默认 true；复选框和 Shift+Tab 共用持久化报告入口，并输出系统聊天反馈。普通 Tab 及按键重复行为已回归。
- 受击最后一帧结束且队列为空时，仅对当前怪物目标恢复原攻击流程。新动作优先，目标切换、失效、死亡通知、角色死亡和动作重置解除锁定并清除旧追击队列。
- `cmake --build build --target client test_continuousattack test_sdruntimeconfig test_textinput --parallel 8` 通过；客户端产物为 `build/client/src/client.exe`。
- `ctest --test-dir build -R '^(continuousattack|sdruntimeconfig|textinput)/' --output-on-failure`：3/3 通过。增强受击每帧空队列断言后，`ctest --test-dir build -R '^continuousattack/' --output-on-failure`：1/1 通过。
- 测试直接使用真实客户端对象，SDL dummy / software 驱动，在安装客户端目录读取资源与 libpinyin 数据；只缓冲网络消息，未启动连接或游戏主循环。
- 相关文件 `git diff --check` 通过。开发检查中补齐了死亡通知立即取消、取消锁定时清空旧追击队列；离线回归覆盖这两个路径。
- 并行会话正在实现“记住账号”，其文件及共享 CMake 中的追加内容均保留；本功能不修改其实现和记录。
- 真实服务器对打、网络连续受击、界面视觉与重新登录后的服务端配置读取尚未实机验证，见 [验证记录](../verification/20261108-continuous-attack.md)。
