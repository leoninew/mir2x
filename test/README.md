<a id="tests"></a>
# 测试

代码地图、Windows 命令和按改动类型选择测试的方法，参见[开发与测试指南](../readme/development/workflow-and-testing.md)。

C++ 测试套件使用项目 vcpkg 注册表基线中的 Catch2 v3。CMake 的 `catch_discover_tests()` 将各个用例注册到 CTest，并以套件名称作为前缀。测试直接断言行为，不使用标准输出基准文件或 Python 比对包装器。

使用支持的仓库外构建配置编译项目，例如：

```bash
/home/anhong/mir2x/build.py \
    --build-dir=/home/anhong/b_mir2x_vcpkg \
    --parallel=10 \
    --c-compiler=gcc-16 \
    --cxx-compiler=g++-16 \
    --assets-path=/home/anhong/mir2x_assets \
    --build-type=Debug \
    --no-ccache
```

测试可执行文件不包含在普通构建中。在上述 `build.py` 命令中追加 `--run-test`，即可编译全部测试并运行调用 CTest 的 CMake `check` 目标。

重新运行全部用例，或选择一个套件：

```bash
ctest --test-dir /home/anhong/b_mir2x_vcpkg/build --output-on-failure
ctest --test-dir /home/anhong/b_mir2x_vcpkg/build -R '^queststate/' --output-on-failure
```

Lua 任务回归脚本在 Catch2 可执行文件中使用 vcpkg 的 Lua 库运行，不需要另装系统 Lua 可执行文件。Lua 断言和模拟环境位于 `server/test/unit/test_*quests.lua` 和 `test_questscripts.lua`。脚本失败时，Lua 错误通过 Catch2 报告。同一个可执行文件还使用 `server/test/merchant.lua` 检查全部 NPC 脚本。

服务端 Lua runner 套件从已安装的服务端目录加载脚本。客户端几何测试套件需要已安装的字体和 emoji 资源，以及可正常工作的 SDL 视频后端。上述 `build.py` 命令会在运行测试前执行安装；当已安装的脚本或资源需要更新时，不要使用 `--no-install`。

`MIR2X_TEST_RES_DIR` 可覆盖客户端套件使用的已安装资源目录。可执行文件参数由 Catch2 管理，因此不使用位置参数传递资源路径。将来新增外部命令集成测试时，CTest 会直接运行这些测试，并根据超时和进程退出状态判断结果。
