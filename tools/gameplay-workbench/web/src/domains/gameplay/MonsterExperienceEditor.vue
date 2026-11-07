<template>
  <WorkbenchTable
    v-model:search="search"
    label="基础经验"
    search-label="搜索怪物"
    search-placeholder="名称或 ID"
    count-label="个怪物"
    empty-text="没有匹配的怪物，试试其他名称或 ID。"
    table-class="min-w-[460px]"
    :columns="columns"
    :rows="rows"
    :total="model.monsters.length"
    :row-key="(entry) => entry.row.monster_id"
  >
    <template #actions><slot name="actions"></slot></template>
    <template #cell-id="{ row: entry }">{{ entry.row.monster_id }}</template>
    <template #cell-name="{ row: entry }">{{ entry.row.monster_name }}</template>
    <template #cell-exp="{ row: entry }">
      <IntegerField
        v-model="entry.row.exp"
        :path="`monsters.${entry.index}.exp`"
        :label="`${entry.row.monster_name} 基础经验`"
        :error="errors[`monsters.${entry.index}.exp`]"
        @edit="$emit('edit', $event)"
      />
    </template>
  </WorkbenchTable>
</template>

<script setup lang="ts">
  import { computed, ref } from "vue";
  import IntegerField from "@/components/IntegerField.vue";
  import WorkbenchTable from "@/components/WorkbenchTable.vue";
  import { matchesSearch, type TableColumn } from "@/components/workbenchTable";
  import type { Monsters } from "@/services/gameplayApi";

  const model = defineModel<Monsters>({ required: true });
  defineProps<{ errors: Record<string, string> }>();
  defineEmits<{ edit: [path: string] }>();
  const search = ref("");
  const columns: TableColumn[] = [
    { key: "id", label: "怪物 ID", class: "w-28 text-slate-500" },
    { key: "name", label: "怪物名称" },
    { key: "exp", label: "基础经验", class: "w-60", required: true },
  ];
  const rows = computed(() =>
    model.value.monsters
      .map((row, index) => ({ row, index }))
      .filter(({ row }) => matchesSearch(search.value, row.monster_name, row.monster_id)),
  );
</script>
