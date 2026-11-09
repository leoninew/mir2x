# Plan / 计划：Alt 临时高亮掉落与名称
最后修改时间: 2026-11-09 07:55:38

## Review status / 审查状态

Accepted

## Intent basis / 意图依据

- [意图](../intent/20261109-alt-drop-highlight.md)，A01–A06。
- 基线 `ccbb8e4`；已有练级指南相关暂存文档保留，不修改其内容或 index。

## Technical decisions / 技术设计与决策

- `SDLDevice` 提供只读的当前窗口键盘焦点查询；`ProcessRun::draw` 每帧读取 SDL 左右 Alt 的实际键盘状态，结合焦点得出本帧临时显示状态，不维护按下/抬起锁存变量。
- 同一个帧状态传给掉落图标、旋转星标和名称绘制。有效高亮是现有高亮规则或 Alt 临时状态，隐藏规则优先。
- 从 `drawGroundItem` 分离名称绘制，在场景、魔法、飘字绘制后、GUI 绘制前执行；非 Alt 状态仍只显示鼠标格的掉落名称。
- 同格名称按地面物品列表顺序纵向排列，居中放在该格上方的深色底板内；只为中心点位于窗口内的格子绘制名称，按实际文字尺寸计算底板和间距，将落点限制在窗口边缘内。不增加点击命中区域。
- 只修改显示，不调用运行时配置保存或发送入口，不拦截现有按键事件；已有 Alt+H/F/E 等组合键沿用原入口。

## Implementation steps / 实施步骤

1. 增加窗口焦点查询和每帧临时高亮状态，并贯通图标、星标（A01、A04）。
2. 独立绘制掉落名称，实现同格排列、底板、窗口边缘限制及绘制层级（A02、A03）。
3. 审查隐藏、松开恢复和现有输入路径，完成客户端构建与已有输入测试，回报实际检查及未验证项（A01–A05）。
4. 按后续安装授权执行文档中的 `runtime` 组件安装，并比较构建和安装客户端的 SHA256（A06）。

## Files to change / 修改范围

- `client/src/processrun.{cpp,hpp}`：临时状态、掉落高亮及名称绘制。
- `client/src/sdldevice.{cpp,hpp}`：当前窗口键盘焦点查询。
- 本功能的意图、计划、验证记录。

## Tasks and coordination / 子任务与协调

不适用。由本会话实施；进展和交付追加到本计划，不调用并行 agent。

## Verification plan / 检查与验证安排

- A01、A04：审查每帧键盘/焦点读取、隐藏优先及高亮叠加，确认不存在配置保存、网络发送及按键事件截获。
- A02、A03：审查同格名称的实际文字尺寸、纵向间距、边缘限制和绘制调用顺序。
- A05：沿用 UCRT64 CMake 构建 `client` 和已有 `test_textinput`；运行 `ctest --test-dir build -R '^textinput/' --output-on-failure`，检查相关 diff。不新增 UI、性能或覆盖率测试。
- A06：执行 `cmake --install build --component runtime`，核对 `install/client/client.exe` 与构建客户端相同；复用已有资源。
- 实机视觉、左右 Alt、Alt+Tab 和掉落重叠场景留给实际客户端验收；未运行时明确记录。

## Risks and recovery / 风险与恢复

- 本期保证同格名称排列；极密集的相邻格名称仍需实机查看可读性。
- 不保存临时状态，角色换图和重建场景不会继承临时锁定。修改无需配置或数据库迁移，可单独回退本功能源码。

## User review notes / 用户审查记录

- 按用户明确实施授权接受上述范围与方案，先完成全部源码改动，再运行相关构建及开发检查；不安装制品或操作游戏角色。
- 用户后续明确要求构建到 `install/`，沿用 [Windows 开发文档](../../readme/development/windows-development.md)中的组件安装方式，不启动游戏。

## Implementation delivery / 实现交付

- 完成四条客户端源码变更：每帧读取左右 Alt 和本窗口焦点，向图标、星标、名称传递同一临时状态；隐藏优先，松开恢复原规则，未改原按键事件处理。
- 名称独立于场景绘制，位于 GUI 下方；同格名称按实际尺寸纵向排列，使用黄色文字与半透明深色底板，并过滤视野外格子、调整窗口边缘落点。
- `cmake --build build --target client test_textinput --parallel 8` 首次及边界修正后增量构建均通过；`ctest --test-dir build -R '^textinput/' --output-on-failure` 为 1/1，通过耗时 1.50 秒。
- 安装前 `cmake --build build --target client --parallel 8` 返回 `ninja: no work to do.`；`cmake --install build --component runtime` 退出 0，更新 `install/client/client.exe`，服务端程序显示 Up-to-date。
- 构建和安装客户端 SHA256 相同：`108C36ECDBF7E3B3806F36B2F13F3A770AACDC804503369C8B1169180F7A9C09`。
- 原始日志保存在 `build/verification/20261109-alt-drop-highlight/`。源码 diff 检查通过；未新增测试或操作 Git index，原有练级指南暂存变更保留。
- 未启动真实客户端或服务端，视觉和按键实机效果待验收，见[验证记录](../verification/20261109-alt-drop-highlight.md)。
