# Verification / 验证：玩法倍率与工作台优化
最后修改时间: 2026-11-07 08:51:06

## 审查状态

Accepted。合并原倍率验收与后续工作台布局、配置/命令验收。依据 [意图](../intent/20261107-gameplay-workbench.md) 和 [计划](../plan/20261107-gameplay-workbench.md)，最终 prod 工作台已构建安装，相关检查通过。

## 意图、计划与集成对齐

新增独立倍率文件、schema、原生启动快照与经验/掉落接入；扩展工作台文档域、保存和备份机制。原基础经验/掉落/刷怪配置、旧窗口倍率存储、PeerConfig 协议及未被消费的 experienceRate 保持原语义。默认 1、小数、0、有效倍率叠乘和重启读取符合约定。

工作台按运行、四类玩法编辑、配置备份、迁移记录拆分，使用共享侧栏、保留草稿的单一状态实例和独立保存栏。补充嵌套键修复、六个 Make 业务入口、默认帮助、外部 profile 选择及 Cygwin 构建安装。过程文档统一为当前同名 intent/plan/verification 三份。

不适用子任务协调。最终差异基于 `d3bb0c1`；保留既有启动器和备份功能，迁移 baseline 无变更。本次未暂存或提交。

## 修复与范围

| 对齐依据 | 发现的偏差 | 实际修复 | 复验 |
| --- | --- | --- | --- |
| A02 | 零值与极大值可能触发原金币拆分断言或非法整数转换 | 零倍率显式短路，经验保留击杀事件，正数超限饱和至 int 上界 | 原生 436 个断言通过 |
| A07/A08 | 启动/管理挤在单页，保存栏覆盖内容，切页可能重建草稿 | 页面与编辑器拆组件；列表单独滚动，保存栏保留空间，共享草稿状态 | 13 项 gameplay UI 测试及编辑页截图 |
| A09 | Dynaconf 新增 gameplay 分支保留 ROOT 大写，Pydantic 拒绝额外字段 | 覆盖写入时统一嵌套键小写 | 配置回归与实际 dev 后端请求通过 |
| A10 | 初版把 profile 写入默认 YAML，不符合用户澄清 | YAML 原样打包；profile 仅选择 dotenv，制品启动钩子提供可覆盖的默认值 | 10 项配置/profile 测试及制品解包核对通过 |
| A04/A10 | Cygwin PATH 找不到 cmake；首次安装 EXE 被正在运行的工作台占用 | 复用主构建缓存的 CMAKE_COMMAND；窗口结束后重新执行正常安装 | 最终 make binary profile=prod exit0，安装哈希一致 |

修复限于已接受范围。未强制结束用户工作台，未启动真实游戏，未修改游戏数据库。

## 验收结果

| 编号 | 结果 | 证据 |
| --- | --- | --- |
| A01 | 通过 | 独立默认三个 1；原生缺文件读取、API 首次创建及独立 UI 测试 |
| A02 | 通过 | 默认迁移基线；生产掉落函数的叠乘/零值分支；经验取整、零值、溢出饱和测试 |
| A03 | 通过 | 保存不改基础文件、missing revision 冲突、单文件备份、含/不含倍率的整目录还原、API 保存后原生 inspect 读取一致 |
| A04 | 通过 | 原生服务端/工具前阶段构建安装通过；最终 prod 工作台安装成功；本次 803 个既有玩法、启动配置及备份文件哈希无变化 |
| A05 | 通过 | Ardot 文件 744582965039840：编辑页 4:1、运行页 4:57；截图和布局检查，原画板保留 |
| A06 | 通过 | 七类页面与分组导航；组件切页测试、桌面编辑页截图，窄屏顶部横向导航样式 |
| A07 | 通过 | 1920×855 实际编辑页：表格独立滚动，保存栏无覆盖，ID/经验列宽固定 |
| A08 | 通过 | 13 项 gameplay UI 测试，覆盖切页保留草稿、空搜索、还原确认及原有冲突/保存行为 |
| A09 | 通过 | `.env.dev` 的 GAMEPLAY__ROOT 解析成功；默认 dev 后端启动后 workspace 请求返回源 server 目录 |
| A10 | 通过 | make 空目标帮助；六个业务目标；prod 前端/EXE 构建安装；源码与制品 YAML 字节一致且无 profile；制品默认 prod、显式 dev 可覆盖 |

## 检查结果

| 命令 / 检查 | 实际结果 |
| --- | --- |
| 前阶段 cmake 构建 test_gameplayconfig、test_gameplaydrop、gameplay_config_tool、server | 成功；对应倍率原生代码此后未修改 |
| test_gameplayconfig / test_gameplaydrop | 4 用例 412 断言 / 1 用例 24 断言通过 |
| `make check`，经全局 `cygwin.bat` 调用 | mypy 30 源文件、Ruff、前端类型/ESLint/Prettier 全部通过；最后 profile 修复后再次通过 |
| `make test` | Python 76 通过、2 项原生联动按默认环境跳过；Vue 9 个文件 41 项通过 |
| `pytest tests/test_config.py tests/test_profiles.py`，最后配置调整后 | 10 通过 |
| `pytest tests/test_gameplay.py`，设置 MIR2X_GAMEPLAY_TOOL | 19 通过，无跳过，补齐 make test 的两项可选原生联动 |
| `MIR2X_WORKBENCH_SERVER__PORT=18082 make webapi` | dev 启动、reload 生效；GET /api/gameplay/workspace 返回 200、bound=true 和源 server 路径 |
| `make` | 只打印帮助，列出六个业务目标及 binary 的 prod 覆盖示例 |
| `make binary profile=prod` | 最终 exit0；Vite prod、PyInstaller、CMake workbench 组件安装均成功 |
| 制品运行时 hook / 配置核对 | 从实际 EXE 提取 hook 验证默认 prod、环境 dev 优先；实际打包 YAML + 临时 .env.prod 覆盖正确；未内嵌 dotenv |
| `install/workbench/Mir2xWorkbench.exe --check-workspace install/server` | exit0，无窗口、无开发服务器 |
| 安装前后 SHA256 | 803 个保留文件全部一致；EXE 与构建产物一致 |
| 开发服务器清理 | 本次 18081 预览、18082 后端及 reload 子进程已结束，未重新打开 |
| `git diff --check` | 通过 |

Ardot 截图：`build/744582965039840/screenshot-4_1-20261107_082518736.png`、`screenshot-4_57-20261107_082518736.png`；实际编辑页截图：`C:/Users/leon/.jev-ultrafast-mcp/shots/mir2x-workbench-editor.png`。编辑原型布局检查通过；运行页仅有预期的留白提示。

最终安装 Mir2xWorkbench.exe SHA256：`F96EA3EB95815D64B622924EFDF2C258FC20EE6FCC31B6E94C6A96ADC3229A37`。保留文件清单位于 `build/workbench-layout-install-before.json`。

## 限制与结论

未启动真实游戏或进行完整人工桌面操作，窄屏未做单独像素截图验收；编辑交互由组件测试及桌面尺寸页面截图覆盖。保存后仍需服务端重启，多节点需部署相同文件。旧安装不强行补入新倍率文件，缺省等价三个 1，首次修改保存创建。

首次默认端口 8000 被已打开的安装版工作台占用，因此只读启动验证使用 18082。PyInstaller 报告可选跨平台模块缺失，最终构建和发行自检成功。浏览器连接后来断开，沿用断开前已核对的页面截图；按用户要求关闭开发服务器后未重启。

授权范围已实现并完成相关检查；最终制品已安装，既有玩法数据与备份保留，无剩余阻塞。
