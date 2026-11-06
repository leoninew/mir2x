# FastAPI + Vue + PyWebView 开发手册

需要 Python 3.12、uv、Node.js 22+、Corepack、GNU Make 和 Bash；Windows 还需要 WebView2 Runtime。Windows 可使用 Git Bash 或 Cygwin。

## 安装

```bash
make deps
```

## 开发

桌面窗口与前后端联调：

```bash
make desktop dev=1 profile=dev
```

浏览器模式在两个终端分别启动：

```bash
make webapi profile=dev
make web profile=dev
```

前端 `http://localhost:8081`，后端 `http://127.0.0.1:8000`。窗口关闭时清理本次启动的 Vite 子进程；已存在的 Vite 服务继续运行。

`profile` 默认 `dev`，读取根目录 `.env.dev`。独立后端支持代码重载；桌面模式支持前端热更新，Python 修改后需重启桌面。

成功返回原始 DTO；失败统一 `{ code, error, requestId }`，校验失败为 `422/validation_error`，未知异常为 `500/internal_error`。前端归一化 HTTP、网络、超时、取消和契约错误，保留请求 ID 与重试提示；重试和鉴权由调用方处理。

## 检查

```bash
make check
make check fix=1
make test
```

## 打包

```bash
make binary
```

Windows 入口：`dist/ExampleDesktop/ExampleDesktop.exe`。分发整个 `dist/ExampleDesktop/` 目录，其中包含默认配置、前端资源和 Python 运行环境。在目标 OS/CPU 上构建。

仅运行本地构建后的前端：

```bash
cd web && corepack yarn build && cd ..
make desktop
```

## 配置

- 默认配置：`config/config.yaml`；使用 `MIR2X_WORKBENCH_*` 环境变量覆盖，嵌套字段用 `__` 分隔。
- 使用 `profile=qa` 读取 `.env.qa`，可从 `.env.example` 复制；前后端使用相同 profile。
- 配置优先级：默认 YAML → `.env.<profile>` → 系统环境变量；启动时统一校验。
- 直接运行和打包程序使用 `MIR2X_WORKBENCH_PROFILE`，未设置时默认 `prod`；配置和 `.env.<profile>` 可放在程序旁。
- 独立后端的服务参数：`--host`、`--port`、`--reload`。
- 前端 `corepack yarn dev` 和 `corepack yarn build` 分别默认 `dev`、`prod`，可用 `--mode qa` 覆盖。
- 日志：`src/mir2x_workbench/logging.yaml` 统一配置，UTF-8 文本写入 stdout 和 `logs/mir2x-gameplay-workbench.log`，20 MiB 轮转，保留 5 份；打包程序写在自身目录下。
- 桌面桥接：`web/src/services/desktopBridge.ts`，支持系统信息、窗口操作、通知和经用户确认的 Shell 命令。
- 更新依赖后执行 `uv lock` 或在 `web/` 下执行 `corepack yarn install`，提交对应锁文件。
