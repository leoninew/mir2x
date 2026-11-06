import { createRouter, createWebHistory, type RouteRecordRaw } from "vue-router";

import GameplayPage from "@/domains/gameplay/GameplayPage.vue";
import NotificationCenter from "@/domains/notifications/NotificationCenter.vue";

const routes: RouteRecordRaw[] = [
  {
    path: "/",
    name: "gameplay",
    component: GameplayPage,
  },
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
