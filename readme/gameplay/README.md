# 游戏玩法参考

练级参考的规范文件随 Python MCP 包与独立启动器一并发行。仓库入口如下：

- [练级参考索引](../../tools/mir2x-mcp/src/mir2x_mcp/runtime_assets/leveling.md)
- [地图与怪物](../../tools/mir2x-mcp/src/mir2x_mcp/runtime_assets/leveling-maps.md)
- [装备与正常获取](../../tools/mir2x-mcp/src/mir2x_mcp/runtime_assets/leveling-equipment.md)
- [技能与经验曲线](../../tools/mir2x-mcp/src/mir2x_mcp/runtime_assets/leveling-skills.md)

MCP 宿主先读 `mir2x://knowledge/leveling`，再按需要读取其 `maps`、`equipment`、`skills` 子资源。知识资源离线可读；实际角色状态、附近目标、路径、库存、报价及操作结果仍以游戏查询为准。

发行目录的本入口由规范索引生成，并在同目录提供四份文档；不维护第二份主题内容。
