# Windows 开发环境
最后修改时间: 2026-11-01 21:12:06

[返回知识库](README.md)

本机已经用 MSYS2 UCRT64 完整编译、安装客户端、服务端和资源。所有开发命令都可以从 `D:\SourceCodes\opensource\mir2x` 发起。

上述安装事实来自已有开发环境；不表示每次新源码构建都会自动更新 install。当前 AI/MCP delivery2 的 client/server 已链接、166 项定向回归通过，最终完整 stdio 和真实角色仍未验证，见[验证清单](../../docs/verification/20261031-ai-character-mcp.md)。MCP 启用与人工控制步骤见[使用说明](ai-mcp.md)，本轮另有运行管理增量构建/安装与隔离开发检查，见运行管理验证记录。

用户随后已自行编译；目前生成标识为 `VENGINEERING-20261101192035`，build/install 中 client/server EXE 的哈希已分别核对相同。该用户编译快照保留在原验证清单；本轮又对 Windows 路径与 auto-launch 正常退出做了窄修，build/install 当前对应和实际检查范围在运行管理验证记录，原 166 项日志不改作新入口的通过证据。

## 工具链与目录约定

| 项目 | 本机值 |
| --- | --- |
| 源码和命令起点 | `D:\SourceCodes\opensource\mir2x` |
| MSYS2 | `D:\ProgramFiles\msys64` |
| 环境 | UCRT64，64 位 MinGW，vcpkg `x64-mingw-static` |
| 编译器 | GCC 16.2.0；当前 CMake 要求 C++26 |
| 构建工具 | CMake 4.4.5、Ninja 1.13.2 |
| 构建脚本运行时 | Python 3.14.9 |
| CMake 工作树 | `build/` |
| 安装结果 | `install/client/`、`install/server/`、`install/tools/` |
| vcpkg 源码与下载 | `vcpkg/` |
| 已安装依赖 | `build/vcpkg_installed/` |
| 原始游戏资源 | `build/assets/mir2x_res/` |
| 本机日志、辅助脚本与二进制缓存 | `.vcpkg/`，已被 Git 忽略 |

`build.py --build-dir=.` 的含义是以当前目录为构建根，然后在其中使用 `build/`、`install/` 和 `vcpkg/`，不是把编译产物直接散落到源码目录。

依据：[build.py](../../build.py)、[CMakeLists.txt](../../CMakeLists.txt)、[Windows CI](../../.github/workflows/build.yml)。工具版本是本次验证值，不是要求以后只能使用这些补丁版本。

## 当前机器的快捷构建

在根目录 PowerShell 中执行：

```powershell
& 'D:\ProgramFiles\msys64\usr\bin\bash.exe' --noprofile --norc ./.vcpkg/build-msys2.sh
```

这个本机辅助脚本设置 UCRT64、Ninja、编译器、构建根和二进制缓存，并调用项目的 `build.py`。它位于忽略目录，不会随 Git 克隆分发。需要重建辅助脚本时，使用下一节中的正式构建入口。

辅助脚本接受追加的 `build.py` 参数，例如 `--run-test`。它使用的代理地址是本机约定；迁移到其他电脑时应调整或移除。

## 不依赖本机辅助脚本的构建方法

在 **MSYS2 UCRT64** 终端中准备环境。首次安装 MSYS2 时先完成 `pacman -Syu`，如终端被更新关闭，重新打开后再次检查更新。依赖工具与项目 CI 保持一致：

```bash
pacman -S --needed \
    mingw-w64-ucrt-x86_64-toolchain \
    mingw-w64-ucrt-x86_64-git \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-pkgconf \
    mingw-w64-ucrt-x86_64-python
```

随后在同一终端构建：

```bash
cd /d/SourceCodes/opensource/mir2x
export CMAKE_GENERATOR=Ninja
mkdir -p .vcpkg/binary-cache
export VCPKG_BINARY_SOURCES="clear;files,$(cygpath -m "$PWD/.vcpkg/binary-cache"),readwrite"

python3 ./build.py \
    --build-dir=. \
    --triplet=x64-mingw-static \
    --host-triplet=x64-mingw-static \
    --c-compiler=gcc \
    --cxx-compiler=g++ \
    --parallel=8
```

vcpkg 注册表基线由 [vcpkg-configuration.json](../../vcpkg-configuration.json) 固定，额外移植位于 [ports](../../ports)。本机验证基线为 `9eec002df1cd109f8f104cd966a763daf9751489`。

需要代理时，在当前构建终端设置实际代理地址。代理不是项目运行条件：

```bash
export HTTPS_PROXY=http://127.0.0.1:7890
export HTTP_PROXY=http://127.0.0.1:7890
```

本次曾遇到 MSYS2 Git 在 vcpkg 子进程中初始化 curl 失败，因此本机辅助脚本把 Git for Windows 放在 UCRT64 前面；编译器、Python、CMake、Ninja 仍使用 UCRT64。无需把这条 PATH 约定扩展到整个系统。

## 日常增量构建

在已完整构建过的 UCRT64 环境和仓库根目录中：

```bash
# 编译客户端或服务端；这一步不更新 install 中的程序。
cmake --build build --target client server --parallel 8

# 将构建结果和内容脚本安装到运行目录。
cmake --install build --config Release
```

修改 CMake、编译器、依赖或构建类型时，用 `build.py` 重新配置。增加新 C++ 文件后也应重新配置，因为源文件列表由配置阶段的目录扫描生成。

- 普通业务 C++ 改动可先构建对应 target；公共头文件改动可能同时影响客户端、服务端和工具。
- `server/src/*.lua` 中不少框架脚本通过 `#embed` 编进 C++，修改后需要重编。
- `server/script/` 中的内容脚本由安装步骤复制；修改后至少重新安装并重启加载相应脚本的服务端。
- `--build-type=Debug` 可以切换构建类型，但会改变现有构建树和安装目录中的程序。调试时也需要重新构建实际调试的目标。
- `--no-install` 只生成构建目录中的程序；不要据此认为 `install/` 已更新。
- `--fresh` 会删除整个 `build/`，包括已安装依赖和默认资源克隆。普通改动不需要使用它。

## 从根目录启动游戏

在仓库根目录或安装目录的 PowerShell 执行：

```powershell
./mir2x.cmd doctor
./mir2x.cmd up
./mir2x.cmd status
```

CLI 自动选择游戏工作目录、等待本工具登记的服务器持有玩家监听端口，再打开可见客户端。默认玩家端口 7000、节点端口 8000；桥接和 standard 策略已提供，首次不用手填 JSON/绝对路径。游戏账号按原界面登录，已有数据库不一定包含新建数据库的默认测试账号，CLI 不保存或自动填入密码。

```powershell
./mir2x.cmd client restart --policy readonly
./mir2x.cmd logs server
./mir2x.cmd stop
```

正常关闭超时先处理窗口提示，明确需要强停时才用 `stop --force`；只管理工具登记并核对身份的进程。需要远端服务时配置可选的 `mir2x.local.toml`，只使用 `client start`。端口也可在该 local TOML 的 network 节覆盖，更新安装保留此文件。

Windows 构建默认增加 `mir2x_runtime` 便携 EXE 目标，client 构建会带上它；正常安装会输出运行入口/配置/skill/文档，单独安装命令为 `cmake --install build --component runtime`。构建端需要 uv；安装制品中的独立 EXE 运行端无需 Python。

LLM 对话与 MCP 注册见[使用说明](ai-mcp.md)和[随包速查](../../tools/mir2x-mcp/src/mir2x_mcp/runtime_assets/QUICKSTART.md)。本轮运行管理的编译、真实只读/关闭范围与当前哈希见[独立检查记录](../../docs/verification/20261101-runtime-launcher.md)，原 166 项历史回归不改标为新客户端入口已复测。

## 制品与存档的区别

新程序在 `install/`，此前下载的程序在 `D:\Downloads\mir2x-windows-build-bb6a222`，是两套独立目录。修改源码不会自动更新下载目录。

服务端数据库文件名是 `mir2x.db3`，相对于服务端进程工作目录。用本文命令启动时，它位于 `install/server/mir2x.db3`。重装程序通常不会删除这个运行时文件，也不会自动把旧结构迁移到新结构。

不要在服务器运行时用资源复制或目录清理命令覆盖存档。需要保存一份开发存档时，先正常停止该服务端，再备份数据库及存在的 SQLite 伴随文件。游戏数据结构的更改见 [业务模型与数据](domain-and-data.md#结构变更与存档兼容)。
