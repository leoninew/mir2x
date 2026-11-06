# 玩法配置

这是基础经验、金币/物品掉落及地图刷怪数据的运行时来源。服务端在 master/slave 启动时完整校验并读取一次；保存后须重新启动相应服务端进程，GUI 不需要与游戏同时运行。

| 文件 | 用途 |
| --- | --- |
| `manifest.json` | 格式版本及掉落/刷怪文件索引 |
| `monsters.json` | 全部有效怪物的基础经验，按数字 ID 区分重名 |
| `drops/<monster_id>.json` | 有序掉落条目，保留 group、概率分母、repeat、count |
| `spawns/<map_id>.json` | 有序刷怪组/区域，保留重复怪物组 |
| `catalog.json` | 与当前游戏编译期身份一致的只读目录 |
| `migration-notes.json` | 历史未生效声明及原始来源，只读，不参与计算 |
| `schema/` | 格式/基础约束说明；引用校验同时由 C++/Python 实现 |

配置为 UTF-8 标准 JSON，拒绝重复键、未知字段、非法整数和未知身份。不要修改 ID/名称配对，不要将名称当作唯一身份。数组顺序有业务意义。

掉落 `prob_recip` 是原概率分母，保留既有倍率；group=0 逐项尝试，其他组最多命中一项。repeat 沿原顺序展开；金币拆分与随机波动仍由原算法处理。刷怪 count=0 沿原最少 1 规则，不表示关闭；time 单位为秒，`cratio` 是未被原算法消费的只读历史字段。经验只迁移基数，原伤害分配、最低值与取整保持。

管理入口：`tools/gameplay-workbench`，选择源码或安装目录中的 **server 文件夹**。也可在启动前手工编辑，再运行：

```text
gameplay-config validate <server>/config/gameplay
gameplay-config catalog
gameplay-config inspect <gameplay-dir> <monster-id> <map-id>
```

`catalog` 从当前编译记录导出身份，不包含旧玩法默认值。新增编译身份应同时明确更新 catalog 和经验全表，不能将新 catalog 静默覆盖用户正在编辑的数据。

初始迁移为 364 怪物、4886 条有效掉落（展开 5402 项）、251 地图/3322 有效区域；386 条旧无效掉落和 6 个无效刷怪组保存在 migration-notes。一次性迁移证据位于 `tools/gameplayconfig/migration/baseline.json`。`export_baseline.py` 只能用于冻结的迁移前源码，不能对已迁移源码重导出覆盖基线。旧 dropitemconfig.inc/dropgen 不再是运行时权威，怪物 CSV 生成器不再输出 exp。

新安装交付完整目录。已有目录先校验再整体保留，包括用户调整的数值；不完整/不兼容则停止安装，不拼入默认文件。更新版本前先备份整个目录并显式迁移。GUI 每文件保存前备份到 `.backups/`，原子替换并检查读取 revision；不提供跨文件事务或多编辑器排他锁。多个服务节点应部署相同配置快照。
