# Plan / 计划：技能书显示学习说明
最后修改时间: 2026-11-08 18:02:21

## Review status / 审查状态

Accepted

## Intent basis / 意图依据

- [技能书显示学习说明](../intent/20261108-skill-book-descriptions.md)，用户要求实际职业与学习说明。
- `SDItem::getXMLLayout()` 和拍卖详情分别读取空的 `ItemRecord::description`，导致两处都显示开发占位语。
- 当前 126 种技能书均能通过普通书名称或秘籍 `.book.magic` 关联到三职业的 63 项 MagicRecord。
- 用户追加要求每种技能用一句话解释用途；本修订覆盖原有“不改技能记录”的限制，仅增加说明字段。

## Technical decisions / 技术设计与决策

- 在现有 SDItem 中新增 `getXMLDescription()`，返回可放入 layout 的段落；普通提示和拍卖详情都调用此方法。
- 对技能书读取关联 MagicRecord，显示技能、适用职业、技能等级要求、前置技能及使用方式。普通书为教头誊抄材料，秘籍在背包使用学习。
- 沿前置链取最高首阶段等级，仅当高于当前技能记录等级时补一行前置链等级要求；遍历上限使用技能表长度，避免错误数据无限循环。
- 技能书已有自定义描述时保留；其他物品沿用原描述和占位策略。不改物品记录或学习判断。
- 在 MagicRecord 末尾添加 `description`，为三职业 63 项技能各填简短作用说明；按最新反馈扩充到一到两句，补充场景、范围特点或关键代价。普通书与秘籍通过同一关联读取，在技能名后显示“技能作用”。其他技能参数保持原值。
- 参考新浪传奇 3 三职业技能页，概括作用、攻击范围类型和关键代价；不照搬故事、具体数值或版本等级。文档职业表增加“技能作用”列，并注明来源及用途与当前实现状态的区别。

## Implementation steps / 实施步骤

1. 实现共享描述片段并接入普通提示、拍卖详情，对应 A01–A03。
2. 在已有物品描述测试文件补充全技能书元数据关联与少量具体示例，覆盖三职业、两种用途及道士前置链，不增加 UI 测试。
3. 使用 UCRT64 构建 client/server 和受影响测试，安装 runtime，再运行物品描述定向检查与 diff-check；记录安装文件散列，对应 A04。
4. 本轮扩展先补齐 63 条用途、共享显示和现有定向 case，再整体构建安装；核对 126 本书都有对应用途及三职业代表文本，对应 A05。旧阶段日志保留，本轮证据单独记录。
5. 用户要求再丰富说明后，只扩充 63 条 description 与文档同名行，复用已有逐书完整文本检查；全部文案完成后构建安装，使用 `expanded-` 前缀记录本轮结果。

## Files to change / 修改范围

- `common/src/sditem.hpp`、`common/src/sditem.cpp`：共享描述生成及普通提示调用。
- `client/src/gui/auctionboard/auctionitemdetaillower.cpp`：拍卖详情复用共享描述。
- `common/test/unit/test_itemquality.cpp`：沿用已有物品描述检查入口。
- `common/src/magicrecord.hpp`、`common/src/magicrecord.inc`：技能用途字段与 63 条文本。
- `readme/gameplay/leveling-skills.md`：说明查看技能书学习要求的入口，职业表补用途和来源。
- 本功能 Intent、Plan、Verification 文档。

## Tasks and coordination / 子任务与协调

不适用；主会话单 writer 完成。

## Verification plan / 检查与验证安排

- 先执行 `cmake --build build --target client server test_itemquality --parallel 8` 及 `cmake --install build --component runtime`。
- 安装后运行 `test_itemquality.exe` 的物品描述相关 case，核对全部 126 种书无开发占位语、职业/等级/前置正确。
- 以雷电术、普通基本剑术、秘籍基本剑术和道士前置链作具体文本检查；保留既有矿石/肉类描述检查。
- 所有书关联的用途非空且出现在完整提示中，具体核对基本剑术、雷电术和治愈术，避免只验证学习信息。
- 比对 build/install 中 client/server SHA256，运行 `git diff --check`；不启动游戏或重打缺失的原始地图资源。

## Risks and recovery / 风险与恢复

- 共享头文件可能引发较多增量编译；只构建本期所需目标。
- 新说明只展现已有规则，不根据角色当前职业或已学技能动态变色；本任务没有角色状态输入。
- 若文本或 XML 检查失败，在相同范围修复后重新构建受影响目标并安装，再复验。

## User review notes / 用户审查记录

- 本次请求授权实现技能书说明；结合既有先安装后验证指令连续完成。Git 和现场操作不在范围内。
- 本轮追加用途说明无需再次审批；保留用户已暂存版本，新改动仅在 working tree，完成后核对 index 字节未变。

## Implementation delivery / 实施交付

### 第一轮：学习信息（历史记录）

- 已实现 `SDItem::getXMLDescription()` 并接入完整物品提示及拍卖详情。126 种书均使用既有技能元数据，补齐职业、技能等级要求、前置技能、前置链较高等级及不同使用方式。
- `client server test_itemquality` 构建成功（168 项，exit 0）。随后 runtime 安装在覆盖 `install/server/server.exe` 时因 Permission denied 返回 1；只读核对确认该文件和 `install/client/client.exe` 均有运行中的游戏进程占用。未关闭游戏或绕过文件锁，安装项待用户退出后继续。
- 构建完成、安装尝试之后运行定向描述检查，2 cases / 870 assertions 全通过；其中枚举全部 126 种书，另含雷电术和道士前置链具体断言及原矿石/肉类说明检查。没有将构建通过记为安装完成。
- 技能/物品记录、服务端学习代码和既有书店库存无差异，`git diff --check` 通过。源/新制品/旧安装制品散列记录于 `build/verification/20261108-skill-book-descriptions/artifact-review.json`。
- 最终修改范围与计划一致；未运行 UI、性能或全量游戏测试，未暂存/提交。验收记录保持 Draft，待实际安装后核对 build/install 一致性。

### 第二轮：一句话技能作用

- 按用户追加要求，在 MagicRecord 增加用途字段，为战士 13 项、法师 27 项、道士 23 项技能各维护一句话；126 本普通书和秘籍通过共享描述入口显示。
- 文案参考新浪传奇 3 三职业技能页并自行简写，游戏提示不夹带研究说明；文档职业表新增技能作用和参考来源，同时更新受数据行偏移影响的源码定位链接。
- 原技能参数、学习代码、物品记录及书店库存保持原值；已有描述 case 增加逐书用途与三职业代表文本检查。原始暂存 diff 与 index 条目散列已保存，本轮不操作暂存区。
- 最终 client/server/test_itemquality 共 201 项构建成功；runtime 正常安装成功，build/install 两端散列一致。安装后定向 2 cases / 1126 assertions 全通过；本轮证据使用 `one-line-` 前缀，前一轮安装失败与旧测试记录不覆盖。最终 Verification 已更新。

### 第三轮：丰富技能说明

- 用户反馈一句话过短；63 条说明全部扩充为两句，每项 36–53 字，补充实际用途、范围特点、使用场景或关键代价。文档 63 个同名条目同步更新。
- 产品增量仅为 MagicRecord 的描述文字，沿用原有共享显示及现有逐书完整文本检查，没有修改技能参数、显示逻辑或测试源码。
- client/server/test_itemquality 共 196 项构建成功，随后 runtime 安装成功；安装后原定向 2 cases / 1126 assertions 全通过。本轮证据使用 `expanded-` 前缀，前两轮记录保留，原暂存区未改变。
