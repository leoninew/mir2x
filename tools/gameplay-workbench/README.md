# Mir2x Gameplay Workbench

基于 Python/FastAPI、pywebview/WebView2、Vue 3/Vite 的桌面玩法配置管理应用。当前管理基础经验、金币/物品掉落和地图刷怪；保存后下次启动服务端生效。

“全局倍率”是独立页签，单独保存 `server/config/gameplay/multipliers.json`，经验、金币、掉率默认均为 1，支持小数和 0。金币/掉率叠乘原服务端倍率，经验乘原击杀经验结果；0 分别关闭对应收益或全部掉落。不会改写基础配置或原服务端窗口设置。旧目录缺少新文件时按 1 读取，首次修改保存创建文件；整目录备份/还原同时包含此文件，旧备份仍兼容。

Windows 需要 WebView2 Runtime；开发需要 Python 3.12+、uv、Node.js 22+、Corepack，Makefile 入口还需要 GNU Make/Bash。

## 使用

正式入口：仓库主安装目录中的 `install/workbench/Mir2xWorkbench.exe`。工作台与同级 `client/`、`server/` 一起交付；工作台自身的 `_internal/` 目录需要保留。

打开后按 `game-launch.json` 的 `server_config_path` 自动加载玩法配置，预置 `../server`，无需每次选择目录。相对路径以 EXE 所在目录解析；可以在「路径设置」中修改，下次打开工作台生效，也可临时选择其他 **server 文件夹**。升级安装保留已有配置。三个视图分别管理经验全表、单怪物掉落文件、单地图刷怪文件；搜索不会重排底层数组。身份/名称及未消费的 cratio 只读；历史未生效记录保留原始来源。

整数错误显示在对应字段；保存前可查看变更。保存包含字段/引用校验、旧内容备份、同目录原子替换。外部修改、目录切换或元数据版本变化会拒绝旧请求，并保留草稿，可下载后人工比较或重新读取。切换文件会在本次窗口内保留草稿；退出前请保存或下载，草稿不持久化。

单文件编辑的历史副本位于所选 `server/config/gameplay/.backups/`。完整玩法目录快照集中保存在工作台目录 `backups/gameplay/`；打开“备份管理”可按时间戳查看、创建和还原，创建时可填写说明。备份记录来源服务端目录，只能还原到原目录。还原会先自动备份当前配置并校验快照，只替换 `server/config/gameplay`，不会涉及角色数据库；未保存草稿需在确认时丢弃。还原后的数据在服务端下次启动时生效。多文件编辑不提供事务；与其他编辑器同时保存仍存在最终替换前的竞争窗口，建议单人编辑。

## 启动客户端与服务端

主页面提供客户端/服务端启动按钮；在「路径设置」中修改相对路径并保存。路径基准是 **Mir2xWorkbench.exe 所在目录**，开发模式以工作台项目目录为基准。默认布局沿用主程序的 install：

```text
install/
  workbench/Mir2xWorkbench.exe
  client/client.exe
  server/server.exe
```

本地配置保存在 EXE 旁的 `game-launch.json`，也可以自行修改文件后在「路径设置」中重新读取：

```json
{
  "schema_version": 1,
  "client_path": "../client/client.exe",
  "server_path": "../server/server.exe",
  "server_config_path": "../server"
}
```

三个路径均支持 `..`、中文、空格及 Windows 反斜杠。client_path/server_path 填写相对 `.exe` 路径，不附命令行参数；server_config_path 填写包含 `config/gameplay` 的服务端目录，可与启动程序目录分别设置。每个进程以自己的程序目录作为工作目录。程序或配套资源不会被自动复制、安装或构建。程序缺失时不能启动；配套资源缺失会显示提示，请自行准备完整运行目录。

旧 `game-launch.json` 会在打开桌面应用时补充 server_config_path，保留已有启动路径；仅首次迁移读取旧 gameplay.root（若存在）。此后桌面配置目录只使用 JSON 中的值，不再由 YAML 或环境变量覆盖。首次安装使用仓库 `config/game-launch.json` 预置文件，已有本地 JSON 不被安装覆盖。配置目录不可用时仍可打开工作台修改路径。

启动使用磁盘配置；未保存的玩法草稿会先提醒，配置管理目录与待启动服务端目录不同时也会提示。服务端只打开原管理窗口，需在该窗口手动启动服务，不传 `--auto-launch`。进程创建成功不代表游戏/服务已就绪，具体错误仍查看游戏自身日志。

状态只追踪本次工作台启动的进程，点击刷新更新；不扫描或关闭外部进程，不保证跨工作台单实例。关闭工作台不会结束游戏。启动与启动路径保存仅走受信桌面桥接，浏览器模式不支持，没有 HTTP 启动接口。本地路径配置及无效文件修复备份不进入版本控制或发行包。

## 开发与检查

```bash
make deps
make check
make test
make web
make webapi
make binary
make binary profile=prod
```

直接运行 `make` 打印帮助。业务目标只保留这六个，默认统一 `profile=dev`：`test` 运行前后端单元测试，`web` 启动前端（8081），`webapi` 启动后端（8000），`binary` 打包并安装到主构建树的安装目录。`make binary profile=prod` 覆盖前端构建模式，并让制品默认选择 `.env.prod`；profile 不写入 YAML，制品通过启动钩子提供默认选择，显式环境变量仍可覆盖。默认 YAML 原样打包，不打包 `.env`。前后端分别在两个终端运行。用应用自身设置 `MIR2X_WORKBENCH_GAMEPLAY__ROOT` 指定测试 server 根；目录选择仅通过受信桌面桥接开放，不提供任意文件路径写入的 HTTP API。

Python distribution 为 `mir2x-gameplay-workbench`，模块为 `mir2x_workbench`，入口为 `mir2x-workbench`。`uv.lock` 与 `web/yarn.lock` 随依赖调整同步。

## 构建

```bash
make binary
uv build
```

先按仓库 `build.py` 配置主构建树。工作台是同一构建树的 `workbench` target，可直接在仓库根目录执行，不需要 GNU Make：

```bash
cmake --build build --target workbench
```

该目标构建完成后自动安装到 `install/workbench/`，无需再手动执行安装命令。开发模式 `.env.dev` 预设源码服务端相对路径 `../../server`。

Windows 程序图标直接复用客户端资源 `client/src/winicon.png`；打包时由 Pillow 转换，无需维护副本。

正式 `build.py` 同样支持 `--target workbench`（沿用主构建的其他参数）；安装目标使用该树的 `CMAKE_INSTALL_PREFIX`，默认仓库 `install/`。未构建工作台时完整安装明确跳过它，显式安装 `--component workbench` 缺包会报错。

更新已有游戏运行目录时，可分别安装 `runtime`（原生程序/脚本）和 `gameplay-config`（完整默认目录首次安装、已有目录校验保留）组件，复用现有资源；`resources` 组件重新打包资源时要求源资源完整。

工具目录的 `make binary` 使用同一个 `build_desktop.py`，不维护第二套打包参数；生成中间包后自动安装 workbench 组件。中间包位置为主构建树的 `workbench/Mir2xWorkbench/`。主构建树不是默认 `build` 时传入 `NATIVE_BUILD_DIR`。Windows 安装目标为 `install/workbench/`，不覆盖已有 `game-launch.json`、`.env` 或日志。工作台安装不操作用户DB。

先构建前端再打 wheel；wheel 包含前端、默认 YAML 和日志资源。editable 安装可直接使用 Vite，不要求预先生成 dist。二进制包含前端、默认配置和 Python 运行环境，不内嵌 `.env`、所选游戏数据或用户备份。

`Mir2xWorkbench.exe --check-workspace <server-root>` 可在不开窗口时检查发行资源和玩法配置，成功 exit0，失败 exit1。

联动原生检查时设置 `MIR2X_GAMEPLAY_TOOL` 为构建后的 `gameplay-config.exe`，运行 `pytest tests/test_gameplay.py`；覆盖共用契约样本以及真实API保存三领域→C++加载，含中文和空格路径。

## 应用设置

应用自身配置与玩法文件分离。始终加载 `config/config.yaml`，再叠加 `.env.<profile>`，最后叠加 `MIR2X_WORKBENCH_*` 环境变量；嵌套字段用 `__`。profile 只选择覆盖文件，由 `MIR2X_WORKBENCH_PROFILE` 指定，开发默认 dev，二进制默认采用构建时所选值；不属于 YAML 配置。可在 EXE 旁放置 `config/config.yaml` 和对应的 `.env.<profile>`。UTF-8轮转日志写入应用目录 `logs/`。

沿用模板通用开发桥接；玩法界面使用目录选择、固定配置 API 和固定游戏启动方法，不使用通用 Shell 执行。格式与运行语义见 [gameplay README](../../server/config/gameplay/README.md)。
