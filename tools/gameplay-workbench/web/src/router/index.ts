import { createRouter, createWebHistory, type RouteRecordRaw } from "vue-router";

import GameplayPage from "@/domains/gameplay/GameplayPage.vue";
import { navigationGroups, type WorkbenchPage } from "@/domains/gameplay/gameplayNavigation";
import NotificationCenter from "@/domains/notifications/NotificationCenter.vue";

const workbenchPaths: Record<WorkbenchPage, string> = {
  launcher: "launcher",
  gm: "gm",
  monsters: "gameplay/monsters",
  drops: "gameplay/drops",
  spawns: "gameplay/spawns",
  multipliers: "gameplay/multipliers",
  backups: "backups",
  migration: "migration",
};

const routes: RouteRecordRaw[] = [
  {
    path: "/",
    redirect: { name: "launcher" },
  },
  ...navigationGroups.flatMap((group) =>
    group.items.map((item) => ({
      path: `/${workbenchPaths[item.page]}`,
      name: item.page,
      component: GameplayPage,
      meta: { workbenchPage: item.page },
    })),
  ),
  {
    path: "/notifications",
    name: "notifications",
    component: NotificationCenter,
  },
];

export const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes,
});
