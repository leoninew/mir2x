<template>
  <section class="flex min-h-0 min-w-0 flex-1 flex-col gap-4" aria-label="GM 命令速查">
    <div class="grid shrink-0 gap-3 rounded-lg border border-slate-200 bg-white p-3 sm:p-4">
      <div class="flex min-w-0 flex-col gap-3 lg:flex-row lg:items-center">
        <label class="min-w-0 flex-1">
          <span class="sr-only">搜索 GM 命令</span>
          <input
            v-model="query"
            class="table-search px-3"
            type="search"
            placeholder="搜索命令、用途、地图、物品或参数"
            aria-label="搜索 GM 命令"
          />
        </label>
        <label class="flex shrink-0 items-center gap-2 text-sm text-slate-600">
          <span>用途</span>
          <select
            v-model="category"
            class="h-9 min-w-36 rounded-md border border-slate-300 bg-white px-2 text-sm"
            aria-label="按用途筛选"
          >
            <option value="all">全部用途</option>
            <option v-for="item in GM_CATEGORIES" :key="item" :value="item">{{ item }}</option>
          </select>
        </label>
      </div>

      <div class="flex flex-wrap items-center gap-1" aria-label="命令入口筛选">
        <button
          v-for="tab in entryTabs"
          :key="tab.id"
          type="button"
          class="rounded-md px-3 py-2 text-sm transition-colors"
          :class="
            entry === tab.id
              ? 'bg-indigo-50 font-medium text-indigo-700'
              : 'text-slate-600 hover:bg-slate-50'
          "
          :aria-pressed="entry === tab.id"
          @click="entry = tab.id"
        >
          {{ tab.label }} <span class="ml-1 text-xs text-slate-500">{{ tab.count }}</span>
        </button>
        <span class="ml-auto px-2 text-xs text-slate-500" aria-live="polite">
          {{ filteredCommands.length }} / {{ GM_COMMANDS.length }} 条
        </span>
      </div>

      <details class="border-t border-slate-100 pt-2 text-xs text-slate-600">
        <summary class="w-fit font-medium">入口与权限</summary>
        <dl class="mt-3 grid gap-3 sm:grid-cols-2 xl:grid-cols-4">
          <div v-for="item in Object.values(GM_ENTRIES)" :key="item.label">
            <dt class="font-medium text-slate-800">{{ item.label }}</dt>
            <dd class="mt-1 leading-5">{{ item.usage }}</dd>
          </div>
          <div class="sm:col-span-2 xl:col-span-4">
            <dt class="font-medium text-slate-800">GM 身份</dt>
            <dd class="mt-1 leading-5">
              源码中的 uidf::isGM() 以玩家 DBID ≤ 10 判断；当前 @、客户端 Lua
              调试入口及相关协议处理没有统一 GM 鉴权。不要将这些命令视为已受 GM 身份保护。
            </dd>
          </div>
        </dl>
      </details>
    </div>

    <div class="grid min-h-0 min-w-0 flex-1 gap-4 lg:grid-cols-[17rem_minmax(0,1fr)]">
      <nav
        class="flex max-h-60 min-w-0 flex-col overflow-auto rounded-lg border border-slate-200 bg-white p-2 lg:max-h-none"
        aria-label="命令列表"
      >
        <button
          v-for="item in filteredCommands"
          :key="item.id"
          type="button"
          class="grid w-full gap-1 rounded-md px-3 py-2.5 text-left"
          :class="
            selected?.id === item.id
              ? 'bg-indigo-50 text-indigo-900'
              : 'text-slate-700 hover:bg-slate-50'
          "
          :aria-current="selected?.id === item.id ? 'true' : undefined"
          @click="selectedId = item.id"
        >
          <span class="truncate font-mono text-sm font-medium">{{ item.name }}</span>
          <span class="truncate text-xs text-slate-500">{{ item.description }}</span>
        </button>
        <p v-if="filteredCommands.length === 0" class="p-4 text-sm text-slate-500">
          没有匹配的命令
        </p>
      </nav>

      <article
        v-if="selected"
        class="flex min-h-0 min-w-0 flex-col gap-5 overflow-auto rounded-lg border border-slate-200 bg-white p-4 sm:p-6"
        aria-live="polite"
      >
        <header class="grid gap-2 border-b border-slate-100 pb-4">
          <div class="flex flex-wrap items-center gap-2">
            <h2 class="min-w-0 break-all font-mono text-xl font-semibold text-slate-950">
              {{ selected.name }}
            </h2>
            <span class="rounded bg-slate-100 px-2 py-1 text-xs text-slate-600">
              {{ GM_ENTRIES[selected.entry].label }}
            </span>
            <span class="rounded bg-indigo-50 px-2 py-1 text-xs text-indigo-700">
              {{ selected.category }}
            </span>
          </div>
          <p class="text-sm leading-6 text-slate-600">{{ selected.description }}</p>
        </header>

        <section class="grid gap-2" aria-label="命令语法">
          <h3 class="text-xs font-medium text-slate-500">语法</h3>
          <code
            v-for="syntax in selected.syntax"
            :key="syntax"
            class="block overflow-x-auto rounded-md bg-slate-50 px-3 py-2 font-mono text-sm text-slate-800"
          >
            {{ syntax }}
          </code>
        </section>

        <section v-if="selected.parameters?.length" class="grid gap-2" aria-label="参数">
          <h3 class="text-xs font-medium text-slate-500">参数</h3>
          <div class="overflow-x-auto rounded-md border border-slate-200">
            <table class="data-table min-w-[34rem]">
              <thead>
                <tr>
                  <th>名称</th>
                  <th>类型</th>
                  <th>说明</th>
                </tr>
              </thead>
              <tbody>
                <tr v-for="parameter in selected.parameters" :key="parameter.name">
                  <td class="font-mono text-xs">{{ parameter.name }}</td>
                  <td class="text-xs">{{ parameter.type }}</td>
                  <td class="text-xs">{{ parameter.description }}</td>
                </tr>
              </tbody>
            </table>
          </div>
        </section>

        <section class="grid gap-3" aria-label="示例">
          <h3 class="text-xs font-medium text-slate-500">示例</h3>
          <div
            v-for="example in selected.examples"
            :key="example.label"
            class="min-w-0 rounded-md border border-slate-200"
          >
            <div class="flex min-w-0 items-center justify-between gap-2 px-3 py-2">
              <p class="min-w-0 truncate text-xs text-slate-600">{{ example.label }}</p>
              <button
                type="button"
                class="table-action shrink-0"
                :aria-label="`复制：${example.label}`"
                @click="copyExample(example.code)"
              >
                <Copy class="size-3.5 shrink-0" aria-hidden="true" />
                复制
              </button>
            </div>
            <pre
              class="overflow-x-auto border-t border-slate-100 bg-slate-50 p-3 font-mono text-xs leading-5"
            ><code>{{ example.code }}</code></pre>
            <p v-if="example.template" class="px-3 py-2 text-xs text-amber-800">
              示例含占位值，请先替换为当前在线角色、怪物或物品的实际 ID。
            </p>
          </div>
        </section>

        <section v-if="selected.notes?.length" class="grid gap-2" aria-label="注意事项">
          <h3 class="text-xs font-medium text-slate-500">注意事项</h3>
          <ul class="grid list-disc gap-1 pl-5 text-sm leading-5 text-slate-600">
            <li v-for="note in selected.notes" :key="note">{{ note }}</li>
          </ul>
        </section>

        <footer class="mt-auto border-t border-slate-100 pt-3">
          <p class="text-xs text-slate-400">实现位置</p>
          <p class="mt-1 break-all font-mono text-xs leading-5 text-slate-500">
            {{ selected.source }}
          </p>
        </footer>
      </article>
      <div
        v-else
        class="grid min-h-56 place-items-center rounded-lg border border-dashed border-slate-300 bg-white text-sm text-slate-500"
      >
        调整搜索词或筛选条件
      </div>
    </div>
  </section>
</template>

<script setup lang="ts">
  import { Copy } from "@lucide/vue";
  import { computed, ref } from "vue";
  import { useToastsStore } from "@/stores/toasts";
  import {
    filterGMCommands,
    GM_CATEGORIES,
    GM_COMMANDS,
    GM_ENTRIES,
    type GMEntry,
  } from "./gmCommands";

  const toasts = useToastsStore();
  const entry = ref<GMEntry | "all">("chat");
  const category = ref("all");
  const query = ref("");
  const selectedId = ref("chat-help");
  const entryTabs = computed(() => [
    { id: "all" as const, label: "全部入口", count: GM_COMMANDS.length },
    ...Object.entries(GM_ENTRIES).map(([id, item]) => ({
      id: id as GMEntry,
      label: item.label,
      count: GM_COMMANDS.filter((command) => command.entry === id).length,
    })),
  ]);
  const filteredCommands = computed(() =>
    filterGMCommands(GM_COMMANDS, entry.value, category.value, query.value),
  );
  const selected = computed(
    () =>
      filteredCommands.value.find((command) => command.id === selectedId.value) ??
      filteredCommands.value[0] ??
      null,
  );

  async function copyExample(code: string): Promise<void> {
    try {
      await navigator.clipboard.writeText(code);
      toasts.show("示例已复制", "success");
    } catch {
      toasts.show("无法访问剪贴板，请手动选择示例复制", "warning");
    }
  }
</script>
