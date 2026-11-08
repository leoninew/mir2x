# Plan / 计划：书店出售低等级技能秘籍
最后修改时间: 2026-11-08 17:08:13

## Review status / 审查状态

Accepted

方案调查完成，可按此直接实施。用户的“记录任务并实现”及后续“直接构建 install，构建完再去验证你要验证的”授权覆盖实现、构建安装与之后的窄范围验证，不含 Git staging/commit 或现场操作。

## Intent basis / 意图依据

- [意图：书店出售低等级技能秘籍](../intent/20261108-direct-low-level-skill-manuals.md)；用户要求把 26 级及以下技能秘籍直接放入普通书店供购买，并接受道士前置限制。
- `common/src/magicrecord.inc` 三职业技能记录首阶段等级 `<= 26` 共 32 项：战士 4、法师 17、道士 11。
- `common/src/itemrecord.inc` 已定义这 32 项对应秘籍，且物品记录价均为 1,000。所有现有 NPC 技能书店都在各自的 `goods` 清单中声明库存。
- 四家书店使用 `server/script/npc/include/merchant/bookseller.lua::setBookseller(spec)`，道馆书鬼使用独立 `setNPCSell` 和对话。共享模块提供 `withSkillManuals(goods)` 合并函数，两种入口均调用它，使五家新增秘籍库存一致并保留书鬼原对话。
- 玩家学习由 `server/src/player.cpp::Player::consumeBook()` 处理，仅识别带 `.book.magic` 的秘籍，并继续验证职业、最低等级、前置和重复学习。道士幽灵盾及神圣战甲术保留现有前置链，不因上架改变。

## Technical decisions / 技术设计与决策

- 在共享 Lua `bookseller` 模块中定义一份明确的 32 个秘籍物品名清单；`withSkillManuals(goods)` 生成新数组，供 `setBookseller` 销售分支和书鬼独立销售入口调用。
- 复制既有货品再追加新货品，不修改调用方 `spec.goods`，不重复插入商品，不影响非销售型书籍说明或 `trade` 行为。
- 不动态从 `MagicRecord` 生成清单：脚本目前没有读取技能职业和首阶段等级的统一 Lua API。显式清单可审阅、可验证；新增技能时需同步更新该表与数据检查。
- 保留现有三个普通首技书作为任务材料。新上架物品使用已有 `（秘籍）` item record；购买本身不直接授予已学技能。
- 账单价格取已有秘籍记录价，不改 `common/src/itemrecord.inc`。

## Implementation steps / 实施步骤

1. 在 `bookseller.lua` 加入 32 项静态秘籍库存及合并函数；四家共享入口和书鬼独立入口均复用此函数，保留各自普通书和对话。验收 A01–A03。
2. 更新 `readme/gameplay/leveling-skills.md` 普通书店表和解释，列明五家书店均新增 32 种秘籍、保留的三种普通书以及购买后仍须满足学习条件。验收 A04。
3. 按 Windows 开发说明配置 UCRT64 环境，先执行 `install` 构建目标。完成后使用源码解析对照 MagicRecord 等级/职业、ItemRecord 秘籍标记/价格和五家书店调用，运行已有 merchant contract、安装脚本一致性、文档链接与 `git diff --check`。不运行全量游戏测试或 live 操作。验收 A01–A05。

## Files to change / 修改范围

- `server/script/npc/include/merchant/bookseller.lua`：集中维护目录并附加到五家已有书店库存。
- `server/script/npc/道馆_1.书鬼_1.GLOC_418_96_0.LOOK_18.lua`：独立售卖入口复用共享目录，保留其原处理器。
- `server/test/merchant.lua`：已有检查兼容书鬼的独立对话，核对五家实际登记库存均保留原书并新增 32 本秘籍。
- `readme/gameplay/leveling-skills.md`：同步用户查阅的供货和学习规则。
- `readme/gameplay/skills-and-books.md`：同步直接购买入口，避免旧完整货表说明产生歧义。
- `docs/intent/20261108-direct-low-level-skill-manuals.md`、本计划与 `docs/verification/20261108-direct-low-level-skill-manuals.md`：SpecFlow 过程记录。

除书鬼售卖入口外不改其他 NPC 调用文件，不改物品/技能记录、C++、数据库 schema 或其他游戏系统。

## Tasks and coordination / 子任务与协调

不适用：共享模块的一份目录、文档同步及直接源数据审查构成一个窄实施节点；无并行任务或共享写协调需求。

## Verification plan / 检查与验证安排

- 从 `MagicRecord` 读取三职业 63 项技能，按 `req.level[0] <= 26` 推导清单，逐项对比共享目录：精确 32 项、职业计数 4/17/11、无遗漏/额外。
- 从 `ItemRecord` 核对每项 `name + （秘籍）` 存在、`book.magic` 对应技能且价格为 1,000；确认原三种普通书仍在店铺入口配置中。
- 检查四个 `bookseller.setBookseller()` 调用点有 `goods`，书鬼独立 `setNPCSell()` 同样调用合并函数；用现有 merchant contract 检查验证 NPC 模块加载/登记。
- 先执行 `install` 构建，之后核对源脚本和安装脚本字节一致；核对 `consumeBook()` 和道士技能 `.req.prior` 未改变，运行已有 merchant 检查、文档链接与 `git diff --check`。不操作运行中游戏。

## Risks and recovery / 风险与恢复

- 风险是静态清单和技能记录未来可能失配；本次通过逐技能元数据比对确保准确。后续技能表新增低于/等于 26 级技能时，维护该目录并重复对照。
- 若 Lua 目录检查失败，修正共享目录再重跑关联检查。回退限于移除共享目录及合并步骤，不触及 NPC 物品记录或学习代码。

## User review notes / 用户审查记录

- 用户要求记录 SpecFlow 任务并实现；开始实施前目标、32 项阈值、普通书保留、秘籍仍受原学习校验等决策均已明确。此计划状态为 Accepted。
- 用户后续明确要求构建 `install` 后再验证，按此顺序执行；初次构建遇到工具链启动异常后按 Windows 开发说明补齐 UCRT64 的进程级 PATH。

## Implementation delivery / 实施交付

- 已实现 32 项共享目录及库存合并函数；四家通用书店和道馆书鬼均接入，原普通书、对话和学习规则保留。两份技能获取文档已同步，保留任务开始前已有的练级/技能调研内容。
- 使用 UCRT64 进程级 PATH 后，`ninja -C build -j 8 install` 完成原生编译，随后在资源打包阶段因缺少 `build/assets/mir2x_assets/map` 返回 1。按 `readme/development/windows-development.md` 的日常更新流程，`runtime` 与 `gameplay-config` 组件安装均返回 0；保留本地现有玩法配置和资源。没有将完整资源安装失败记为成功。
- 安装之后才执行检查。现有 merchant 检查最初因其只支持共享对话入口而在书鬼处失败；适配原检查后通过 9 类商店契约、83 个 NPC 脚本和五家实际书店库存。32 项秘籍与技能/物品元数据全量对齐，三职业数量为 4/17/11、记录价格均为 1,000。
- 8 个源/构建与安装文件散列匹配，包括 client/server、共享库存脚本及五家 NPC 脚本；两份文档共 180 个本地链接有效。`client server test_questscripts` 增量构建返回 0，原文 `ninja: no work to do.`；`git diff --check` 返回 0。
- 完整证据位于忽略目录 `build/verification/20261108-direct-low-level-skill-manuals/`；验收详情见[验证记录](../verification/20261108-direct-low-level-skill-manuals.md)。未暂存或提交，未启动/重启服务端或客户端；完整资源重打包仍缺原始地图目录。
