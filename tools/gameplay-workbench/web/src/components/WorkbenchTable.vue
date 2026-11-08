<template>
  <section class="flex min-h-0 min-w-0 flex-1 flex-col gap-3 overflow-hidden" :aria-label="label">
    <div class="table-toolbar" role="search" :aria-label="`${label}搜索`">
      <slot name="filters"></slot>
      <div class="relative block w-56 shrink-0">
        <span class="sr-only">{{ searchLabel }}</span>
        <Search
          class="pointer-events-none absolute top-1/2 left-3 size-4 -translate-y-1/2 text-slate-400"
          :stroke-width="1.75"
          aria-hidden="true"
        />
        <input
          ref="searchInput"
          v-model="search"
          type="search"
          :aria-label="searchLabel"
          :placeholder="searchPlaceholder"
          :disabled="disabled"
          class="table-search"
        />
        <button
          v-if="search"
          type="button"
          :aria-label="`清除${searchLabel}`"
          :disabled="disabled"
          class="filter-control absolute top-1/2 right-1 -translate-y-1/2"
          @click="clearSearch"
        >
          <X class="size-4" aria-hidden="true" />
        </button>
      </div>
      <span class="shrink-0 text-xs text-slate-500" aria-live="polite">
        {{ rows.length }} / {{ total ?? rows.length }} {{ countLabel }}
      </span>
      <div v-if="$slots.actions" class="ml-auto flex shrink-0 items-center gap-2">
        <slot name="actions"></slot>
      </div>
    </div>
    <div
      class="min-h-0 flex-1 overflow-auto rounded-lg border border-slate-200 bg-white"
      :aria-label="`${label}列表`"
      tabindex="0"
      :aria-busy="loading"
    >
      <table :class="['data-table', tableClass]" :aria-label="label">
        <thead>
          <tr>
            <th
              v-for="column in columns"
              :key="column.key"
              scope="col"
              :class="[column.class, { 'table-actions': column.key === 'actions' }]"
            >
              {{ column.label }}
              <span v-if="column.required" class="required" aria-hidden="true"> *</span>
            </th>
          </tr>
        </thead>
        <tbody>
          <tr v-if="loading">
            <td :colspan="columns.length" class="table-empty">
              <AppLoading label="正在加载列表…" />
            </td>
          </tr>
          <template v-else>
            <tr v-for="(row, index) in rows" :key="rowKey(row)">
              <td
                v-for="column in columns"
                :key="column.key"
                :class="[column.class, { 'table-actions': column.key === 'actions' }]"
              >
                <slot :name="`cell-${column.key}`" :row="row" :index="index">
                  {{ row[column.key as keyof Row] ?? "—" }}
                </slot>
              </td>
            </tr>
            <tr v-if="!rows.length">
              <td :colspan="columns.length" class="table-empty">{{ emptyText }}</td>
            </tr>
          </template>
        </tbody>
      </table>
    </div>
  </section>
</template>

<script setup lang="ts" generic="Row extends object">
  import { Search, X } from "@lucide/vue";
  import { ref } from "vue";
  import AppLoading from "./AppLoading.vue";
  import type { TableColumn } from "./workbenchTable";

  const search = defineModel<string>("search", { default: "" });
  const searchInput = ref<HTMLInputElement | null>(null);
  function clearSearch(): void {
    search.value = "";
    searchInput.value?.focus();
  }
  withDefaults(
    defineProps<{
      label: string;
      columns: TableColumn[];
      rows: Row[];
      rowKey: (row: Row) => string | number;
      total?: number;
      countLabel?: string;
      searchLabel?: string;
      searchPlaceholder?: string;
      emptyText?: string;
      tableClass?: string;
      disabled?: boolean;
      loading?: boolean;
    }>(),
    {
      total: undefined,
      countLabel: "条记录",
      searchLabel: "搜索",
      searchPlaceholder: "搜索名称或 ID",
      emptyText: "没有匹配的记录，试试其他关键词。",
      tableClass: "",
      disabled: false,
      loading: false,
    },
  );
  defineSlots<{
    filters?: () => unknown;
    actions?: () => unknown;
    [name: `cell-${string}`]: (props: { row: Row; index: number }) => unknown;
  }>();
</script>
