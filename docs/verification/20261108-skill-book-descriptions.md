# Verification / 验证：技能书显示学习说明
最后修改时间: 2026-11-08 18:02:21

## Review status / 审查状态

Accepted

## Intent alignment / 意图对齐

- 63 种普通书及对应 63 种秘籍均显示技能作用、实际职业、首阶段角色等级要求、前置技能和使用方式，不再显示开发占位语。按最新反馈，每项技能作用扩充为两句，约 36–53 字。
- 普通书用于教头誊抄，秘籍在背包使用；幽灵盾及神圣战甲术额外列出前置链要求到 27 级。
- 技能作用覆盖战士 13 项、法师 27 项、道士 23 项；普通书与秘籍共用 MagicRecord 的 63 条用途。文案参考新浪传奇 3 三职业技能资料并自行概括，来源已记入练级技能文档。
- 技能记录仅增加说明字段，原参数及学习规则、物品数据保持原值。最终 client/server 已安装到 install，散列与 build 一致。

## Plan alignment / 计划对齐

- 普通物品完整提示和拍卖详情均调用 `SDItem::getXMLDescription()`，背包和 NPC 商店已有完整提示路径自然使用新说明。未增加逐本重复的职业/等级配置。
- 按追加要求扩展 MagicRecord 和已有描述 case，文档职业表增加同样的用途；没有引入新技能规则或其他物品说明。
- 第三轮仅丰富 63 条描述和文档同名行，复用共享显示与现有测试。全部文案完成后统一构建，再安装 runtime、运行定向检查；前两轮的构建、安装和检查记录保留。

## Task deliveries and integration / 子任务交付与集成

不适用。

## Corrections and scope / 修复与范围

- 最终 diff 包含共享描述接口与实现、拍卖接线、技能用途字段与 63 条文本、已有描述测试文件和练级文档，符合修订后的计划。
- 第一轮安装因运行中的 server.exe 被锁而失败；本轮只读检查确认游戏进程已退出，正常安装成功。没有终止游戏进程或绕过文件锁。
- 技能用途参考资料仅用于简短介绍，不作为全部技能运行时实现已验收的证明；文档保留当前施法入口缺口与熟练度边界。
- 用户已有暂存变更保持原样；本轮新增和修改只留工作树，未暂存或提交。

## Acceptance results / 验收结果

| 验收 ID | 结果 | 证据或未满足原因 |
| --- | --- | --- |
| A01 | 通过 | 定向测试遍历全部 126 种书，均无开发占位语。 |
| A02 | 通过 | 逐书核对 MagicRecord 职业/首阶段等级/前置；雷电术 16 级法师及道士 27 级前置链具体断言通过，原技能参数未改。 |
| A03 | 通过（代码/定向测试） | 普通书与秘籍分别显示誊抄和背包使用；背包/商店调用完整提示，拍卖调用相同描述片段。既有矿石/肉类提示检查通过，非书籍原描述逻辑保留；未做 UI 现场验收。 |
| A04 | 通过 | 第三轮 client/server 和测试程序构建、runtime 安装、安装后定向检查均 exit 0；build/install 两端散列相同。 |
| A05 | 通过 | 63 项技能均有两句作用说明和场景、特点或代价补充，逐书检查 126 本普通书/秘籍均包含完整文本；文档 63 行与技能记录逐字一致。基本剑术、雷电术、治愈术的具体句子通过检查。 |

## Check results / 检查结果

证据目录：`build/verification/20261108-skill-book-descriptions/`。

| 命令 / 检查 | 实际结果 | 证据 / 未运行原因 |
| --- | --- | --- |
| `cmake --build build --target client server test_itemquality --parallel 8` | exit 0，196 项完成。 | `expanded-build.log` / `.exitcode`。 |
| `cmake --install build --component runtime` | exit 0。 | `expanded-install-runtime.log` / `.exitcode`。 |
| `test_itemquality.exe` 的技能书和矿石/肉类描述两项 case | 2 cases / 1126 assertions 全部通过，exit 0。 | `expanded-item-descriptions.log` / `.exitcode`，在构建安装后执行。 |
| 技能参数及文档核对 | 去掉新增的 63 条 description 后，技能数据与原版逐字一致；文档 63 条用途与源码一致。 | `expanded-artifact-review.json`。 |
| 源和制品 SHA256 记录 | build/install client 散列相同，server 散列相同；完整 SHA256 已记录。 | `expanded-artifact-review.json`。 |
| 用户原暂存区核对 | cached diff 与 index 全部条目的前后 SHA256 一致。 | `expanded-index-before.json`、`expanded-artifact-review.json`。 |
| `git diff --check` | exit 0。 | `expanded-diff-check.exitcode`。 |

前两轮历史证据保留：第一轮 `build.log` 为 168 项构建成功，`install-runtime.log` 为文件锁导致 exit 1，`item-descriptions.log` 为 2 cases / 870 assertions 通过；第二轮 `one-line-build.log` 为 201 项成功，`one-line-install-runtime.log` 安装成功，`one-line-item-descriptions.log` 为 2 cases / 1126 assertions 通过。对应散列分别见 `artifact-review.json` 和 `one-line-artifact-review.json`，不替代第三轮结果。

## Risks and incomplete items / 风险与未完成项

- 构建、安装及定向描述检查已完成，无待解决的安装阻塞。未启动游戏进行 UI 现场验收，按用户要求不做 UI、性能或全量游戏测试。
- 此次只补充技能介绍，不扩大为技能效果实现或战斗功能验收。未关闭游戏或操作角色，没有 Git 暂存/提交。

## Conclusion / 结论

- 原学习说明和丰富后的 63 条两句技能介绍均已实现，client/server 已安装到 install；最终定向检查通过，符合修订后的 A01–A05。用户原暂存版本未改变，本轮增量尚未暂存。
