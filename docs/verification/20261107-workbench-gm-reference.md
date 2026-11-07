# Verification / 验证：Workbench GM 命令速查

最后修改时间: 2026-11-07 14:29:02

## 审查状态

已完成源码检查、Workbench 桌面构建及安装，冻结程序工作区检查通过。

## 意图与计划对齐

- 在 Workbench 导航加入独立 GM 命令页面，路径为 `/gm`；页面不要求工作区绑定。
- 静态结构化目录包含 29 条可检索参考记录：12 个聊天 `@` 命令、客户端 Lua、服务端 LOCAL/ASYNC 函数。入口与类别筛选、组合文本搜索、详细参数和源码定位均位于前端。
- 页面默认展示聊天命令；Lua 示例仍按独立入口分类。文档 gm-commands.md 仅收录聊天 @ 命令，Lua 用法迁至 lua-debugging.md。当前没有聊天加金币命令，已明确说明。
- 所有示例仅可查看和复制；没有连接桌面桥接、HTTP API、进程/协议发送或任意 Lua 执行入口。
- 更新 GM 命令参考与 Workbench README。源码注册的聊天命令是 12 条；既有研究文档曾误记 13 条，已按当前源码纠正。

## 检查结果

| 检查                                                | 结果                                                        |
| --------------------------------------------------- | ----------------------------------------------------------- |
| `corepack yarn typecheck`                           | 通过                                                        |
| 定点 `corepack yarn eslint ...`                     | 通过                                                        |
| 定点 `corepack yarn prettier --check ...`（Vue/TS） | 通过                                                        |
| 新增 GM 参考及过程文档 Prettier 检查                | 通过                                                        |
| `corepack yarn build`                               | 通过，Vite 处理 743 个 modules                              |
| GM 目录只读 smoke                                   | 通过，29 条记录、12 个 `@` 命令、加钱与地面金币多词搜索命中 |
| `git diff --check`                                  | 通过                                                        |
| GM 参考源码链接                                     | 全部解析到现存文件                                          |

未运行 UI 测试、Python/C++ 测试、原生构建、游戏命令或现场验证，符合范围约束。

## 未完成项与结论

- 用户追加授权后已执行既有 CMake workbench 目标，构建及安装 exit 0；安装到 `install/workbench/Mir2xWorkbench.exe`。
- 安装版 `--check-workspace D:/SourceCodes/opensource/mir2x/install/server` exit 0；安装静态入口与本轮 Vite 输出一致。
- EXE SHA256：`BDA74BC2F3587EDF2478D27A6CEE082B35D43B6CA81AA639324692C1C8D9DFC6`。
- 本轮页面默认入口调整后定点 ESLint 通过；两份玩法文档已格式化，定点 diff-check 通过。未启动 GUI 或游戏。
- 当前 shell 未配置 cmake PATH，使用 CMakeCache 中的 `D:/ProgramFiles/msys64/ucrt64/bin/cmake.exe` 成功构建。
- GM 命令权限边界和 `@makeItem` 缺陷只作事实说明，未在本功能中修复。
- 本次不 stage/commit；保留工作区已有其它改动。
