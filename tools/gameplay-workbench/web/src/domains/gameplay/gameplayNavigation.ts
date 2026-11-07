import type { Domain } from "@/services/gameplayApi";

export type WorkbenchPage = Domain | "launcher" | "backups" | "migration";
export const navigationGroups: {
  label: string;
  items: { page: WorkbenchPage; label: string; description: string }[];
}[] = [
  {
    label: "工作台",
    items: [
      {
        page: "launcher",
        label: "游戏运行",
        description: "启动游戏，查看本次工作台启动的进程状态。",
      },
    ],
  },
  {
    label: "玩法配置",
    items: [
      {
        page: "monsters",
        label: "基础经验",
        description: "按怪物调整击杀基础经验；全局倍率在独立页面设置。",
      },
      {
        page: "drops",
        label: "金币与物品掉落",
        description: "调整掉落条目，保留原有分组与尝试顺序。",
      },
      {
        page: "spawns",
        label: "地图刷怪",
        description: "按地图设置怪物区域、目标数量与刷新周期。",
      },
      {
        page: "multipliers",
        label: "全局倍率",
        description: "独立设置经验、金币与掉率的整体倍率。",
      },
    ],
  },
  {
    label: "管理",
    items: [
      { page: "backups", label: "配置备份", description: "备份已保存的配置，或还原到已有备份。" },
      { page: "migration", label: "迁移记录", description: "查看从历史数据保留的未生效声明。" },
    ],
  },
];

export function isConfigPage(page: WorkbenchPage): page is Domain {
  return ["monsters", "drops", "spawns", "multipliers"].includes(page);
}
