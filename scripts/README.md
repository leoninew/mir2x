# 构建辅助

原生构建的正式入口是仓库根目录的 [build.py](../build.py)。[build-msys2.sh](build-msys2.sh) 封装 UCRT64 环境初始化，并调用同一个构建入口。

在仓库根目录的 MSYS2 UCRT64 终端执行：

```bash
bash ./scripts/build-msys2.sh --help
bash ./scripts/build-msys2.sh
```

默认完成配置、编译和完整安装。已有完整资源时，日常更新客户端、服务端和工作台可执行：

```bash
cmake --build build --target client server --parallel 8
cmake --install build --component runtime
cmake --install build --component gameplay-config
cmake --build build --target workbench
```

最后一条命令打包 Windows 工作台并自动安装到 `install/workbench/`，需要已安装工作台构建依赖。组件职责见 [Windows 开发指南](../readme/development/windows-development.md#日常增量构建)。

工具由当前终端的 `PATH` 提供。MSYS2 安装目录、从 PowerShell 进入 UCRT64 的启动方式及代理由本机终端配置管理；仓库命令使用相对路径。

脚本从仓库根目录运行 `build.py`，默认使用 Ninja、GCC/G++、`x64-mingw-static` 和 8 个并行任务，保留已有构建缓存。可用 `MIR2X_BUILD_JOBS` 或追加 `--parallel=N` 调整并行数；其他参数原样传给 `build.py`，包括 `--build-dir`、`--no-install` 和 `--run-test`。默认仍执行安装，按需使用 `--no-install`。

未设置 `VCPKG_BINARY_SOURCES` 时使用仓库内 `.vcpkg/binary-cache`；已有设置（包括显式空值）原样保留。脚本继承代理环境变量，不指定代理地址，也不修改系统环境。需要 Git for Windows 时，按 [Windows 开发指南](../readme/development/windows-development.md)在当前终端单独设置。

首次安装工具链、资源准备和增量安装步骤见 [Windows 开发指南](../readme/development/windows-development.md)。
