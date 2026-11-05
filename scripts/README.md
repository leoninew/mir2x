# 构建辅助

原生构建的正式入口是仓库根目录的 [build.py](../build.py)。[build-msys2.sh](build-msys2.sh) 将 `develop` 开发指南中使用的本机 UCRT64 构建方法整理为可随仓库分发的辅助入口。

在仓库根目录的 MSYS2 UCRT64 终端执行：

```bash
bash ./scripts/build-msys2.sh --help
bash ./scripts/build-msys2.sh --no-install --target client --target server
```

也可从 PowerShell 调用实际安装的 MSYS2 bash，例如：

```powershell
& 'D:\ProgramFiles\msys64\usr\bin\bash.exe' --noprofile --norc ./scripts/build-msys2.sh --help
```

脚本从仓库根目录运行 `build.py`，默认使用 Ninja、GCC/G++、`x64-mingw-static` 和 8 个并行任务，保留已有构建缓存。可用 `MIR2X_BUILD_JOBS` 或追加 `--parallel=N` 调整并行数；其他参数原样传给 `build.py`，包括 `--build-dir`、`--no-install` 和 `--run-test`。默认仍执行安装，按需使用 `--no-install`。

未设置 `VCPKG_BINARY_SOURCES` 时使用仓库内 `.vcpkg/binary-cache`；已有设置（包括显式空值）原样保留。脚本继承代理环境变量，不指定代理地址，也不修改系统环境。需要 Git for Windows 时，按 [Windows 开发指南](../readme/development/windows-development.md)在当前终端单独设置。

首次安装工具链、资源准备和增量安装步骤见 [Windows 开发指南](../readme/development/windows-development.md)。
