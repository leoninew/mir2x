<template>
  <div class="min-h-screen bg-slate-100 text-slate-950">
    <header class="border-b border-slate-200 bg-white">
      <div class="mx-auto flex max-w-7xl items-center justify-between gap-4 px-4 py-4 sm:px-6">
        <p class="m-0 text-sm font-semibold text-slate-900">Mir2x 工作台</p>
        <nav aria-label="Primary navigation" class="flex items-center gap-2">
          <RouterLink
            to="/"
            class="rounded-md px-3 py-2 text-sm font-medium text-slate-600 transition-colors hover:bg-slate-100 hover:text-slate-950"
            exact-active-class="bg-slate-900 text-white hover:bg-slate-900 hover:text-white"
          >
            玩法配置
          </RouterLink>
        </nav>
      </div>
    </header>

    <main class="mx-auto w-full max-w-7xl px-4 py-6 sm:px-6">
      <RouterView />
    </main>
  </div>
</template>

<script setup lang="ts">
  import { RouterLink, RouterView } from "vue-router";
</script>
