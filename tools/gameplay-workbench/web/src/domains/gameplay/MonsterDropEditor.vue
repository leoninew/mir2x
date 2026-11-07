<template>
  <WorkbenchTable
    v-model:search="search"
    label="掉落条目"
    search-label="搜索掉落物品"
    search-placeholder="物品名称或 ID"
    count-label="个条目"
    :columns="columns"
    :rows="rows"
    :total="items.length"
    :row-key="(entry) => `${entry.document.monster_id}-${entry.index}`"
    :empty-text="items.length ? '没有匹配的掉落物品。' : '没有配置掉落条目。'"
    table-class="min-w-[820px]"
  >
    <template #actions><slot name="actions"></slot></template>
    <template #filters>
      <FilterCombobox v-model="monsterId" label="怪物" :options="monsterOptions" />
      <FilterCombobox v-model="itemId" label="物品" :options="itemOptions" />
      <FilterCombobox v-model="group" label="组" :options="groupOptions" class="w-28" />
    </template>
    <template #cell-id="{ row: entry }">{{ entry.row.item_id }}</template>
    <template #cell-monster="{ row: entry }">{{ entry.document.monster_name }}</template>
    <template #cell-item="{ row: entry }">
      {{ entry.row.item_name }}
    </template>
    <template v-for="field in dropFields" :key="field.key" #[`cell-${field.key}`]="{ row: entry }">
      <IntegerField
        v-model="entry.row[field.key]"
        :path="`${entry.document.monster_id}.items.${entry.index}.${field.key}`"
        :label="`第 ${entry.index + 1} 项 ${field.label}`"
        :minimum="field.key === 'group' ? 0 : 1"
        :error="errors[`${entry.document.monster_id}.items.${entry.index}.${field.key}`]"
        @edit="$emit('edit', $event)"
      />
    </template>
  </WorkbenchTable>
</template>

<script setup lang="ts">
  import { computed, ref, watch } from "vue";
  import FilterCombobox from "@/components/FilterCombobox.vue";
  import IntegerField from "@/components/IntegerField.vue";
  import WorkbenchTable from "@/components/WorkbenchTable.vue";
  import { matchesSearch, type TableColumn } from "@/components/workbenchTable";
  import type { Drops } from "@/services/gameplayApi";

  const model = defineModel<Drops[]>({ required: true });
  defineProps<{
    errors: Record<string, string>;
  }>();
  defineEmits<{ edit: [path: string] }>();
  const search = ref("");
  const monsterId = ref<string | number | null>(null);
  const itemId = ref<string | number | null>(null);
  const group = ref<string | number | null>(null);
  const monsterOptions = computed(() =>
    model.value.map((document) => ({
      value: document.monster_id,
      label: `${document.monster_name}（ID：${document.monster_id}）`,
    })),
  );
  const items = computed(() =>
    model.value.flatMap((document) =>
      document.items.map((row, index) => ({ document, row, index })),
    ),
  );
  const monsterItems = computed(() =>
    items.value.filter(
      (entry) => monsterId.value === null || entry.document.monster_id === monsterId.value,
    ),
  );
  const itemOptions = computed(() => [
    ...new Map(
      monsterItems.value.map(({ row }) => [
        row.item_id,
        { value: row.item_id, label: `${row.item_name}（ID：${row.item_id}）` },
      ]),
    ).values(),
  ]);
  const groupOptions = computed(() =>
    [
      ...new Set(
        monsterItems.value
          .filter(({ row }) => itemId.value === null || row.item_id === itemId.value)
          .map(({ row }) => row.group),
      ),
    ]
      .sort((left, right) => left - right)
      .map((value) => ({ value, label: `组 ${value}` })),
  );
  watch(monsterId, () => {
    itemId.value = null;
    group.value = null;
  });
  watch(itemId, () => {
    group.value = null;
  });
  const dropFields = [
    { key: "group", label: "组" },
    { key: "prob_recip", label: "概率分母" },
    { key: "repeat", label: "尝试次数" },
    { key: "count", label: "数量" },
  ] as const;
  const columns: TableColumn[] = [
    { key: "id", label: "ID", class: "w-28 text-slate-500" },
    { key: "item", label: "物品", class: "min-w-40" },
    { key: "monster", label: "怪物", class: "min-w-36" },
    ...dropFields.map((field) => ({ ...field, class: "w-36", required: true })),
  ];
  const rows = computed(() =>
    monsterItems.value.filter(
      ({ row }) =>
        (itemId.value === null || row.item_id === itemId.value) &&
        (group.value === null || row.group === group.value) &&
        matchesSearch(search.value, row.item_name, row.item_id),
    ),
  );
</script>
