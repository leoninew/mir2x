# Verification / 验证：工作台玩法配置备份与还原
最后修改时间: 2026-11-07 00:23:55

## Review status / 审查状态

Accepted

## Intent alignment / 意图对齐

- 只备份绑定的 `server/config/gameplay`，时间戳命名并保留说明；不包含角色数据库，也不提供任意路径输入。
- 备份仓库位于工作台应用目录 `backups/gameplay`；记录来源服务端目录，其他来源备份不可还原到当前目录。
- 还原自动备份当前配置，校验后替换目录；成功后旋转 workspace ID、清空前端草稿并重新加载。

## Plan alignment / 计划对齐

- 固定工作区 API、SHA-256 文件清单、临时目录原子发布与同卷切换回滚均按计划实现。

## Task deliveries and integration / 子任务交付与集成

无 task。

## Corrections and scope / 修复与范围

- 相对计划补充了来源服务端目录绑定，避免在集中列表中误将一个 server 的备份还原到另一个 server；UI 显示其他来源并禁用还原。
- 独立 `build_app()` 的备份根目录以当前工作目录为基准，桌面运行仍明确使用 EXE/项目应用根目录。

## Acceptance results / 验收结果

| 验收 ID | 结果 | 证据或未满足原因 |
| --- | --- | --- |
| A01 | 通过 | 针对性 API 正常流程验证时间戳名称、说明、目录来源和集中快照。 |
| A02 | 通过 | API 列表与 Vue “备份管理”提供按来源区分的还原入口。 |
| A03 | 通过 | 正常还原流程验证原数据恢复及“还原前自动备份”；快照 SHA-256 和玩法结构在替换前验证。 |
| A04 | 通过 | 还原后生成新 workspace ID；Vue 清理旧草稿并重新读取。 |
| A05 | 通过 | Python/Vue 静态检查、Workbench CMake 构建/安装及发行 CLI 检查通过；未触及角色数据库。 |

## Check results / 检查结果

| 命令 / 检查 | 实际结果 | 未运行原因 |
| --- | --- | --- |
| `uv run --locked --extra desktop pytest -q tests/test_gameplay.py::test_gameplay_backup_create_list_and_restore` | 1 passed | — |
| `uv run --locked --extra desktop ruff check .` | 通过 | — |
| `uv run --locked --extra desktop ruff format --check .` | 33 个文件通过 | — |
| `uv run --locked --extra desktop mypy src tests build_desktop.py` | 31 个源/测试文件通过 | — |
| `corepack yarn lint` | 通过 | — |
| `corepack yarn typecheck` | 通过 | — |
| `corepack yarn build` | 通过；Vite 715 modules | — |
| `cmake --build build --target workbench` | 通过 | 仅构建工作台，不重编 client/server。 |
| `cmake --install build --config Release --component workbench` | 通过 | 仅更新正式 `install/workbench`。 |
| `Mir2xWorkbench.exe --help` | exit 0 | — |
| `Mir2xWorkbench.exe --check-workspace <server>` | exit 0 | 只读检查仓库 server 玩法配置，不修改其文件。 |
| `git diff --check` | 通过 | 有 README CRLF→LF 提示，无空白错误。 |

## Risks and incomplete items / 风险与未完成项

- 没有进行 GUI/UI 测试或真实目录还原；仅对临时 fixture 执行正常创建/列表/还原流程。
- 没有备份删除/数量上限，持续创建会占用磁盘空间。

## Conclusion / 结论

- 玩法配置备份管理已实现并构建安装到主 CMake 安装目录；仍需用户在实际工作台窗口中体验界面。变更未暂存/提交。
## 默认安装与相对路径交付

- `workbench` 目标现默认完成构建和 workbench 组件安装，本轮实际 exit0，正式位置 `install/workbench/Mir2xWorkbench.exe`。
- 工作台自己的 `config/config.yaml` 预置 `gameplay.root: ../server`。加载器以应用根目录解析配置值，没有在业务代码中固定服务端路径；首次安装生成外部配置，已有配置保留。开发 `.env.dev` 预置 `../../server`。
- 定向配置及备份正常流程：4 passed / 7.90s；修改文件 Ruff/format 通过，mypy 31 文件通过。
- 从系统临时目录执行已安装 EXE 的 `--help`、`--check-workspace <install/server>` 均 exit0。
- 源码加载器读取实际安装配置，确认默认绑定安装 server；临时配置改为 `../my-server` 后解析使用该新值，均通过。未做 UI/性能测试或实际数据还原。
- 最终已安装 EXE SHA256：`18D19D4A060F21828B337A5F2A8AEB328E1450874230E69637EF6C43DAAA7FB9`。包含备份功能及最终草稿状态清理。
- 首次 CMake 再生成缺少原构建要求的 CC/CXX 环境而失败；补回原 GCC/G++ 工具链环境后构建安装成功，没有重编 client/server。
