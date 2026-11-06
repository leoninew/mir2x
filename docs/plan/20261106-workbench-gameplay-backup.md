# Plan / 计划：工作台玩法配置备份与还原
最后修改时间: 2026-11-07 00:33:22

## Review status / 审查状态

Accepted

## Intent basis / 意图依据

- `docs/intent/20261106-workbench-gameplay-backup.md`：只操作绑定的 `server/config/gameplay`，名称自动使用时间戳，备份说明可编辑，还原前自动创建安全备份。

## Technical decisions / 技术设计与决策

- 备份仓库固定在工作台应用目录 `backups/gameplay/<uuid>/`，与源玩法配置目录分离。冻结版以 EXE 所在目录为应用目录，桌面开发版沿用工作台项目目录；独立 `build_app()` 以当前工作目录为应用根目录。
- 每个备份包含 `metadata.json` 和 `data/` 完整目录副本。元数据记录 schema 版本、UUID、时间戳名称、创建时间、说明、来源服务端目录、目录清单及逐文件 SHA-256 清单；仅允许还原至原来源目录。
- 只接受普通目录和普通文件；源目录或备份中出现符号链接/特殊文件时拒绝，所有用户路径均由应用内部推导，不接受 API 路径参数。
- 创建先复制到仓库内临时目录，完成玩法 bundle 校验及文件清单后再原子改名发布；异常清理未发布临时目录。
- 还原先校验 UUID、清单、实际文件集、哈希、bundle 与 migration notes；在同一 `WorkspaceService` 锁内创建“还原前自动备份”，将有效快照复制到当前配置目录同级临时目录并校验，然后以同卷目录改名切换。切换失败时把旧目录改回；成功后删除旧目录并更换 workspace ID。
- 备份创建/查看/还原使用固定 `/api/gameplay/backups` API，不提供文件浏览或自定义目标路径。API 实例共享桌面应用确定的备份根目录和玩法工作区锁。
- UI 使用现有 Reka Dialog：列表与创建说明表单放在备份管理弹窗；还原用二次确认。确认文案只说明覆盖范围及（若有）未保存草稿丢弃；成功后前端清空草稿并刷新。

## Implementation steps / 实施步骤

1. 新增严格请求/元数据模型与玩法快照服务，实现安全复制、校验、原子发布及可回滚还原（A01、A03）。
2. 将固定备份根目录接入桌面 API 应用状态，添加列表、创建、还原路由与响应（A01、A02、A04、A05）。
3. 增加前端 API 类型和备份管理弹窗，接入工作区入口、草稿丢弃确认与还原后刷新（A01、A02、A04）。
4. 增加正常路径 API/服务检查，更新工作台 README 与验证记录；完成 Python 静态检查及 Vue lint/typecheck/build（A01-A05）。
5. 所有代码完成后，按主仓库 CMake `workbench` 目标构建，并仅安装 `workbench` 组件到正式 `install/workbench`（A05）。

## Files to change / 修改范围

- `tools/gameplay-workbench/src/mir2x_workbench/gameplay_backups.py`（新增）。
- `tools/gameplay-workbench/src/mir2x_workbench/gameplay_models.py`、`api.py`、`desktop.py`、`routers/gameplay.py`。
- `tools/gameplay-workbench/tests/test_gameplay.py`。
- `tools/gameplay-workbench/web/src/services/gameplayApi.ts`、`composables/useGameplay.ts`、`domains/gameplay/GameplayPage.vue`，及新增备份弹窗组件；按需更新 Vite 桌面桥接类型 mock。
- `tools/gameplay-workbench/README.md`。
- `tools/gameplay-workbench/.gitignore`（排除本地集中备份）。
- `docs/intent/20261106-workbench-gameplay-backup.md`、本计划、`docs/verification/20261106-workbench-gameplay-backup.md`。
- 不修改原生 client/server、玩法配置内容、角色数据库、其他 worktree 或安装目录中的游戏数据。

## Tasks and coordination / 子任务与协调

不适用。

## Verification plan / 检查与验证安排

- Python：对临时复制的玩法 fixture 执行一个正常创建→列表→还原流程；验证说明/时间戳、配置内容恢复和还原前安全备份存在。仅按用户要求做针对性正常路径检查，不增加反向、UI、性能或覆盖率测试。
- Python 静态检查：在 `tools/gameplay-workbench` 运行 `uv run --locked --extra desktop ruff check .`、`uv run --locked --extra desktop ruff format --check .`、`uv run --locked --extra desktop mypy src tests build_desktop.py`。
- 前端：运行 `corepack yarn lint`、`corepack yarn typecheck`、`corepack yarn build`；不运行 UI 测试。
- 发行：确认正式安装目录中确切 Workbench EXE 未运行后，执行 `cmake --build build --target workbench` 和 `cmake --install build --config Release --component workbench`；使用 `--help` 与临时 fixture `--check-workspace` 作轻量发行检查。不得进行游戏现场还原。

## Risks and recovery / 风险与恢复

- 磁盘空间或权限不足会导致备份创建失败；仅发布完整快照，保留当前数据。
- 当前没有应用内删除或数量上限，用户需自行监控工作台目录磁盘占用。
- 目录改名切换中第二步失败时回滚旧目录；失败备份仍保留供手动恢复，API 返回明确错误。
- 中央仓库可看到多个来源目录的备份，但其他来源的记录不可还原到当前目录。
- 还原后的运行时生效时机仍是服务端下次启动，不尝试热加载或重启。

## User review notes / 用户审查记录

- 方案依据用户确认的配置目录和时间戳命名；集中存储按 Workbench 应用目录约定实现。

## 默认安装与路径配置补充

追加实施：workbench target 默认构建后安装自身组件；首次安装生成 workbench/config/config.yaml，已有文件保留。gameplay.root 默认 ../server，以应用根目录解析，开发 .env.dev 使用 ../../server。保留现有图标/Pillow 打包修改。针对性正常路径检查后统一构建安装。

## 路径配置统一（替代前条 YAML 预置方案）

用户要求改进：路径集中在 Workbench 自己的 game-launch.json，新增 server_config_path，默认 ../server，支持 UI 编辑且下次打开生效。旧 JSON 升级保留启动路径，只在缺字段时迁移旧 gameplay.root；以后 JSON 是桌面目录唯一来源。首次安装复制预置 JSON，已有文件不覆盖。目录不可用时允许进入设置修正。检查仅正常迁移、持久化及桌面绑定与必要静态检查，最终统一构建安装；不启动 client/server。
