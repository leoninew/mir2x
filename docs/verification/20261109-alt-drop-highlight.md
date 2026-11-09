# Verification / 验证：Alt 临时高亮掉落与名称
最后修改时间: 2026-11-09 07:55:38

## Review status / 审查状态

Draft：实现、必要开发检查及安装已完成，实机验收未运行。

## Intent and plan alignment / 意图与计划对齐

- 依据[意图](../intent/20261109-alt-drop-highlight.md)和[计划](../plan/20261109-alt-drop-highlight.md)实施，基线 `ccbb8e4`。
- 最终变更为四条客户端源码和本功能三份过程记录。已有练级指南暂存变更保持原样；无 task，不涉及跨会话集成。
- 用户后续要求安装到 `install/`，使用现有 `runtime` 安装组件，未改服务端源码、配置或安装脚本。

## Corrections and scope / 修复与范围

- 开发审查发现地图绘制范围含屏幕外余量；新增名称单独检查格子中心是否位于窗口内，避免视野外名称被挤到边缘。修正后重新构建客户端和输入测试。
- 没有新增持久化配置、动作请求或快捷键拦截。

## Acceptance results / 验收结果

| 验收 ID | 结果与证据 |
| --- | --- |
| A01 | 源码已核：本帧 Alt 状态叠加原高亮，隐藏优先；左右 Alt 的实际游戏效果待验收。 |
| A02 | 源码已核：同格名称纵向排列、深色底板，未按 Alt 时保留鼠标格名称；视觉待验收。 |
| A03 | 源码已核：名称在场景、特效和飘字之后、GUI 之前绘制；实机遮挡效果待验收。 |
| A04 | 源码已核：SDL 当前键盘状态与窗口焦点逐帧读取，无锁存变量及配置写入；Alt+Tab、原快捷键实机待验收。 |
| A05 | 客户端及现有输入测试构建通过，输入回归 1/1 通过；未执行 UI、性能或覆盖率测试。 |
| A06 | `runtime` 安装退出 0，构建及安装客户端 SHA256 一致。 |

## Check results / 检查结果

| 命令 / 检查 | 实际结果 |
| --- | --- |
| `cmake --build build --target client test_textinput --parallel 8` | 首次和最终增量构建退出 0；最终产物 `build/client/src/client.exe`。 |
| `ctest --test-dir build -R '^textinput/' --output-on-failure` | 1/1 通过，总耗时 1.50 秒；为既有输入回归，不代表新功能视觉验收。 |
| `cmake --build build --target client --parallel 8` | 安装前检查通过，`ninja: no work to do.`。 |
| `cmake --install build --component runtime` | 退出 0，安装到 `install/client/client.exe`。 |
| 构建与安装客户端 SHA256 比较 | 相同：`108C36ECDBF7E3B3806F36B2F13F3A770AACDC804503369C8B1169180F7A9C09`。 |
| `git diff --check` | 通过。 |

日志位于 `build/verification/20261109-alt-drop-highlight/`：`build.log`、`build-final.log`、`textinput.log`、`install-build.log`、`install-runtime.log` 及对应已保存的退出码。

## Risks and incomplete items / 风险与未完成项

- 真实游戏内的左右 Alt、释放恢复、窗口切换、同格多件掉落和界面遮挡尚未验收；密集相邻格名称的可读性也待实际查看。
- 未启动游戏、未暂存或提交代码。

## Conclusion / 结论

实现及安装已交付，开发检查通过。实际入口为 `install/client/client.exe`，实机验收保持待办。
