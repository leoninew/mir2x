<template>
  <DialogRoot :open="selected !== null" @update:open="!$event && (selected = null)">
    <WorkbenchTable
      v-model:search="search"
      label="迁移记录"
      search-label="搜索迁移记录"
      search-placeholder="来源、原因或原始内容"
      :columns="columns"
      :rows="rows"
      :total="records.length"
      :row-key="(entry) => entry.index"
      :empty-text="records.length ? '没有匹配的迁移记录。' : '暂无未生效的迁移记录。'"
      table-class="min-w-[820px]"
    >
      <template #cell-domain="{ row: entry }">{{ domainLabel(entry.record.domain) }}</template>
      <template #cell-source="{ row: entry }">
        <span class="break-all font-mono text-xs">{{ entry.record.source_path }}</span>
      </template>
      <template #cell-position="{ row: entry }">{{ entry.record.source_position }}</template>
      <template #cell-reason="{ row: entry }">{{ entry.record.reason }}</template>
      <template #cell-actions="{ row: entry }">
        <button type="button" class="table-action" @click="selected = entry.record">
          查看详情
        </button>
      </template>
    </WorkbenchTable>
    <DialogPortal>
      <DialogOverlay class="fixed inset-0 z-40 bg-slate-950/30" />
      <DialogContent class="workbench-dialog max-w-3xl">
        <DialogTitle class="text-lg font-semibold">迁移记录详情</DialogTitle>
        <DialogDescription class="mt-2 break-all text-sm text-slate-500">
          {{ selected?.source_path }} : {{ selected?.source_position }}
        </DialogDescription>
        <template v-if="selected">
          <p class="mt-4 text-sm text-slate-700">{{ selected.reason }}</p>
          <pre class="mt-4 overflow-auto rounded-lg bg-slate-50 p-4 text-xs leading-6">{{
            JSON.stringify(selected.raw, null, 2)
          }}</pre>
        </template>
        <div class="mt-6 flex justify-end">
          <button type="button" class="secondary-button" @click="selected = null">关闭</button>
        </div>
      </DialogContent>
    </DialogPortal>
  </DialogRoot>
</template>

<script setup lang="ts">
  import {
    DialogContent,
    DialogDescription,
    DialogOverlay,
    DialogPortal,
    DialogRoot,
    DialogTitle,
  } from "reka-ui";
  import { computed, ref } from "vue";
  import WorkbenchTable from "@/components/WorkbenchTable.vue";
  import { matchesSearch, type TableColumn } from "@/components/workbenchTable";
  import type { Catalog } from "@/services/gameplayApi";

  type MigrationRecord = Catalog["migration_notes"]["records"][number];
  const props = defineProps<{ records: MigrationRecord[] }>();
  const search = ref("");
  const selected = ref<MigrationRecord | null>(null);
  const columns: TableColumn[] = [
    { key: "domain", label: "配置类型", class: "w-28" },
    { key: "source", label: "来源文件", class: "w-72" },
    { key: "position", label: "原始位置", class: "w-28 text-slate-500" },
    { key: "reason", label: "保留原因", class: "min-w-64" },
    { key: "actions", label: "操作", class: "w-28" },
  ];
  const domainLabel = (domain: string) =>
    ({ monsters: "基础经验", drops: "物品掉落", spawns: "地图刷怪" })[
      domain as "monsters" | "drops" | "spawns"
    ] ?? domain;
  const rows = computed(() =>
    props.records
      .map((record, index) => ({ record, index }))
      .filter(({ record }) =>
        matchesSearch(
          search.value,
          domainLabel(record.domain),
          record.source_path,
          record.source_position,
          record.reason,
          JSON.stringify(record.raw),
        ),
      ),
  );
</script>
