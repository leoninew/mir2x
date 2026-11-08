# Verification / 验证：书店出售低等级技能秘籍
最后修改时间: 2026-11-08 17:08:13

## Review status / 审查状态

Accepted

## Intent alignment / 意图对齐

- 五家普通技能书店均使用同一目录新增 32 本秘籍：战士 4、法师 17、道士 11；各店原普通首技书完整保留。
- 全部商品使用已有物品记录，基础价均为 1,000 金币。技能、物品及学习代码无修改；道士前置链保持原状。

## Plan alignment / 计划对齐

- 一份共享目录供四家通用书店和书鬼独立入口复用，没有复制五份货表。书鬼保留原对话处理器。
- `leveling-skills.md` 与 `skills-and-books.md` 已说明直接购买途径、原普通书保留、秘籍使用和前置限制。
- 按用户后续指令先构建/安装，再验证。完整资源安装失败后按开发文档采用日常组件安装，实际脚本与 client/server 均已更新到 `install`。

## Task deliveries and integration / 子任务交付与集成

不适用。

## Corrections and scope / 修复与范围

- 修复初版只接入四家共享入口而漏掉书鬼的问题：提供 `withSkillManuals(goods)`，独立售卖入口也调用它，不覆盖书鬼原对话。
- 现有 merchant 检查原先假设引用 merchant 模块的 NPC 必定使用共享对话 setter；书鬼接入库存函数后触发该假设。检查现兼容独立对话，并逐家验证原货品、新秘籍和无重复，不改产品对话以迎合测试。
- 删除文档中“书店只供三个首技”和“首技任务为最短路线”的过时结论，同步另一份技能书指南。

## Acceptance results / 验收结果

| 验收 ID | 结果 | 证据或未满足原因 |
| --- | --- | --- |
| A01 | 通过 | 实际加载五家 NPC 脚本，各店原货品保留，新增 32 本秘籍且无重复；元数据比对无遗漏/额外。 |
| A02 | 通过 | 32 本均为已有技能书物品，`.book.magic` 对应技能，价格 1,000；物品记录无差异。 |
| A03 | 通过 | `magicrecord.inc`、`itemrecord.inc`、`player.cpp`、`npchar.cpp` 无差异；幽灵盾 → 困魔咒 27 级、神圣战甲术 → 幽灵盾保持。 |
| A04 | 通过 | 两份指南已同步，180 个本地链接均存在。 |
| A05 | 本期部署与检查通过 | client/server 和脚本已通过 runtime 组件安装，玩法配置组件安装成功并保留本地配置；安装后商店检查、文件散列和 diff-check 通过。完整 resources 重打包未完成，详见限制。 |

## Check results / 检查结果

| 命令 / 检查 | 实际结果 | 未运行原因 |
| --- | --- | --- |
| `ninja -C build -j 8 install`（UCRT64 PATH） | 原生编译完成；安装资源时 exit 1，缺少 `build/assets/mir2x_assets/map`。 | 完整目标未成功，原日志 `install-build.log` 保留。 |
| `cmake --install build --component runtime` | exit 0；client/server、工具及内容脚本安装完成。 | `install-runtime.log`。 |
| `cmake --install build --component gameplay-config` | exit 0；校验并保留既有本地玩法配置。 | `install-gameplay-config.log`。 |
| `test_questscripts.exe "Merchant contracts and NPC entry paths"` | 修复既有检查入口后 exit 0；9 类商店契约、83 个 NPC 脚本、五家 32 本秘籍库存通过。 | 修复前 `merchant-before.log` / exit 42；修复后 `merchant-after.log` / exit 0。 |
| 技能/物品/五家原货品目录比对 | exit 0；63 项三职业技能中阈值内精确 32 项，4/17/11；全部价格与秘籍关联正确。 | `catalog-installed-validation.log` / `.json`。 |
| 源/构建与安装文件 SHA256 比对 | 8 项全部一致：client、server、共享库存脚本及五家 NPC 脚本。 | `catalog-installed-validation.json`。 |
| 两份技能指南本地链接 | 180 项全部有效。 | `catalog-installed-validation.log`。 |
| `cmake --build build --target client server test_questscripts --parallel 8` | exit 0，`ninja: no work to do.` | `native-no-work.log`。 |
| `git diff --check` | exit 0。 | `diff-check.exitcode`。 |

日志和检查结果统一位于 `build/verification/20261108-direct-low-level-skill-manuals/`，不纳入源码提交。最初未配置 UCRT64 PATH 时出现编译器启动失败及测试进程 `0xC0000139`，补齐当前进程 PATH 后正常运行；未把环境启动失败当成业务测试结果。

## Risks and incomplete items / 风险与未完成项

- 完整 `install` 目标的资源重打包未完成，原因是本地原始资源目录缺 `map`；本轮使用文档规定的 runtime/gameplay-config 日常安装流程复用已安装资源，没有修改或下载资源仓库。
- 未执行游戏现场购买/学习或服务器重启。服务端需在下次加载 NPC 脚本时使用新库存；安装文件一致性不等于当前运行中实例已重载。
- 未构建 Workbench，本功能没有 Workbench 改动。未运行全量游戏测试、UI 或性能测试；未暂存或提交。

## Conclusion / 结论

- 代码、文档及本期运行程序/脚本部署已完成，窄范围检查通过。完整地图资源重打包的本地缺失单独保留，不宣称完整 `install` 目标已成功，也不影响本次已安装的书店 Lua 脚本。
