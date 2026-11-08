<template>
  <aside class="flex min-h-0 flex-col border-b border-slate-200 bg-white md:border-r md:border-b-0">
    <div class="flex items-center justify-between px-5 py-4 md:py-6">
      <p class="text-base font-semibold tracking-tight text-slate-950">Mir2x 工作台</p>
      <button type="button" class="text-button md:hidden" :disabled="busy" @click="$emit('choose')">
        更换目录
      </button>
    </div>
    <nav
      aria-label="工作台导航"
      class="flex gap-1 overflow-x-auto px-3 pb-3 md:min-h-0 md:flex-1 md:flex-col md:gap-5 md:overflow-y-auto md:py-2"
    >
      <div v-for="group in navigationGroups" :key="group.label" class="flex shrink-0 gap-1 md:grid">
        <p class="mb-1 hidden px-3 text-xs text-slate-400 md:block">{{ group.label }}</p>
        <RouterLink
          v-for="item in group.items"
          :key="item.page"
          v-slot="{ href, navigate }"
          :to="{ name: item.page }"
          custom
        >
          <a
            :href="href"
            :aria-disabled="busy || undefined"
            :tabindex="busy ? -1 : undefined"
            :aria-current="page === item.page ? 'page' : undefined"
            :class="[
              'flex items-center gap-3 rounded-md px-3 py-2.5 text-left text-sm whitespace-nowrap transition-colors aria-disabled:opacity-50',
              page === item.page
                ? 'bg-indigo-50 font-medium text-indigo-700'
                : 'text-slate-600 hover:bg-slate-50 hover:text-slate-950',
            ]"
            @click="busy ? $event.preventDefault() : navigate($event)"
          >
            <component
              :is="pageIcons[item.page]"
              class="size-4 shrink-0"
              :stroke-width="1.75"
              aria-hidden="true"
            />
            <span>{{ item.label }}</span>
          </a>
        </RouterLink>
      </div>
    </nav>
  </aside>
</template>

<script setup lang="ts">
  import {
    ArchiveRestore,
    Coins,
    History,
    Map,
    Play,
    Skull,
    SlidersHorizontal,
    Swords,
    Terminal,
  } from "@lucide/vue";
  import type { Component } from "vue";
  import { RouterLink } from "vue-router";
  import { navigationGroups, type WorkbenchPage } from "./gameplayNavigation";

  const pageIcons = {
    launcher: Play,
    "map-guide": Map,
    gm: Terminal,
    monsters: Swords,
    drops: Coins,
    spawns: Skull,
    multipliers: SlidersHorizontal,
    backups: ArchiveRestore,
    migration: History,
  } satisfies Record<WorkbenchPage, Component>;

  defineProps<{ page: WorkbenchPage; busy: boolean }>();
  defineEmits<{ choose: [] }>();
</script>
