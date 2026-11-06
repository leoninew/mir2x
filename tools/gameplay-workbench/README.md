# Mir2x Gameplay Workbench

基于 Python/FastAPI、pywebview/WebView2、Vue 3/Vite 的桌面玩法配置管理应用。当前管理基础经验、金币/物品掉落和地图刷怪；保存后下次启动服务端生效。

Windows 需要 WebView2 Runtime；开发需要 Python 3.12+、uv、Node.js 22+、Corepack，Makefile 入口还需要 GNU Make/Bash。

## 使用

正式入口：仓库主安装目录中的 `install/workbench/Mir2xWorkbench.exe`。工作台与同级 `client/`、`server/` 一起交付；工作台自身的 `_internal/` 目录需要保留。

打开后选择源码或安装目录中的 **server 文件夹**。三个视图分别管理经验全表、单怪物掉落文件、单地图刷怪文件；搜索不会重排底层数组。身份/名称及未消费的 cratio 只读；历史未生效记录保留原始来源。

整数错误显示在对应字段；保存前可查看变更。保存包含字段/引用校验、旧内容备份、同目录原子替换。外部修改、目录切换或元数据版本变化会拒绝旧请求，并保留草稿，可下载后人工比较或重新读取。切换文件会在本次窗口内保留草稿；退出前请保存或下载，草稿不持久化。

备份位于所选 `server/config/gameplay/.backups/`。多文件编辑不提供事务；与其他编辑器同时保存仍存在最终替换前的竞争窗口，建议单人编辑。

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
  "server_path": "../server/server.exe"
}
```

支持 `..`、中文、空格及 Windows 反斜杠；只填写相对 `.exe` 文件路径，不附命令行参数。每个进程以自己的程序目录作为工作目录。程序或配套资源不会被自动复制、安装或构建。程序缺失时不能启动；配套资源缺失会显示提示，请自行准备完整运行目录。

启动使用磁盘配置；未保存的玩法草稿会先提醒，配置管理目录与待启动服务端目录不同时也会提示。服务端只打开原管理窗口，需在该窗口手动启动服务，不传 `--auto-launch`。进程创建成功不代表游戏/服务已就绪，具体错误仍查看游戏自身日志。

状态只追踪本次工作台启动的进程，点击刷新更新；不扫描或关闭外部进程，不保证跨工作台单实例。关闭工作台不会结束游戏。启动与启动路径保存仅走受信桌面桥接，浏览器模式不支持，没有 HTTP 启动接口。本地路径配置及无效文件修复备份不进入版本控制或发行包。

## 开发与检查

```bash
make deps
make desktop dev=1 profile=dev
make check
make test
```

浏览器开发模式分别运行 `make webapi profile=dev` 与 `make web profile=dev`。用应用自身设置 `MIR2X_WORKBENCH_GAMEPLAY__ROOT` 指定测试 server 根；目录选择仅通过受信桌面桥接开放，不提供任意文件路径写入的 HTTP API。

Python distribution 为 `mir2x-gameplay-workbench`，模块为 `mir2x_workbench`，入口为 `mir2x-workbench`。`uv.lock` 与 `web/yarn.lock` 随依赖调整同步。

## 构建

```bash
make binary
uv build
```

先按仓库 `build.py` 配置主构建树。工作台是同一构建树的 `workbench` target，可直接在仓库根目录执行，不需要 GNU Make：

```bash
cmake --build build --target workbench
cmake --install build --config Release --component workbench
```

正式 `build.py` 同样支持 `--target workbench`（沿用主构建的其他参数）；安装目标使用该树的 `CMAKE_INSTALL_PREFIX`，默认仓库 `install/`。未构建工作台时完整安装明确跳过它，显式安装 `--component workbench` 缺包会报错。

更新已有游戏运行目录时，可分别安装 `runtime`（原生程序/脚本）和 `gameplay-config`（完整默认目录首次安装、已有目录校验保留）组件，复用现有资源；`resources` 组件重新打包资源时要求源资源完整。

工具目录的 `make binary` / `make package` 只是同一个 `build_desktop.py` 的便捷入口，不维护第二套打包参数。`binary` 构建后安装workbench组件，`package`仅构建。

`make package` 仅生成中间包，位置为主构建树的 `workbench/Mir2xWorkbench/`（默认仓库 `build/workbench/Mir2xWorkbench/`）。主构建树不是默认 `build` 时传入 `NATIVE_BUILD_DIR`。Windows 安装目标为 `install/workbench/`，不覆盖已有 `game-launch.json`、`.env` 或日志。工作台安装不操作用户DB。

先构建前端再打 wheel；wheel 包含前端、默认 YAML 和日志资源。editable 安装可直接使用 Vite，不要求预先生成 dist。二进制包含前端、默认配置和 Python 运行环境，不内嵌 `.env`、所选游戏数据或用户备份。

`Mir2xWorkbench.exe --check-workspace <server-root>` 可在不开窗口时检查发行资源和玩法配置，成功 exit0，失败 exit1。

联动原生检查时设置 `MIR2X_GAMEPLAY_TOOL` 为构建后的 `gameplay-config.exe`，运行 `pytest tests/test_gameplay.py`；覆盖共用契约样本以及真实API保存三领域→C++加载，含中文和空格路径。

## 应用设置

应用自身配置与玩法文件分离。优先级为默认 `config/config.yaml` → `.env.<profile>` → `MIR2X_WORKBENCH_*` 环境变量；嵌套字段用 `__`，默认 profile 为 prod。可在 EXE 旁放置 `config/config.yaml` 和 `.env.prod`。UTF-8轮转日志写入应用目录 `logs/`。

沿用模板通用开发桥接；玩法界面使用目录选择、固定配置 API 和固定游戏启动方法，不使用通用 Shell 执行。格式与运行语义见 [gameplay README](../../server/config/gameplay/README.md)。
