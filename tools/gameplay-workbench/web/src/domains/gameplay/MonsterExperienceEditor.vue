<template>
  <table class="config-table experience-table">
    <thead>
      <tr>
        <th>怪物 ID</th>
        <th>怪物名称</th>
        <th>基础经验 <span class="required">*</span></th>
      </tr>
    </thead>
    <tbody>
      <tr v-for="entry in rows" :key="entry.row.monster_id">
        <td class="text-slate-500">{{ entry.row.monster_id }}</td>
        <td>{{ entry.row.monster_name }}</td>
        <td>
          <IntegerField
            v-model="entry.row.exp"
            :path="`monsters.${entry.index}.exp`"
            :label="`${entry.row.monster_name} 基础经验`"
            :error="errors[`monsters.${entry.index}.exp`]"
            @edit="$emit('edit', $event)"
          />
        </td>
      </tr>
      <tr v-if="!rows.length">
        <td colspan="3" class="py-10 text-center text-slate-500">
          没有匹配的怪物，试试其他名称或 ID。
        </td>
      </tr>
    </tbody>
  </table>
</template>

<script setup lang="ts">
  import { computed } from "vue";
  import IntegerField from "@/components/IntegerField.vue";
  import type { Monsters } from "@/services/gameplayApi";

  const model = defineModel<Monsters>({ required: true });
  const props = defineProps<{ search: string; errors: Record<string, string> }>();
  defineEmits<{ edit: [path: string] }>();
  const rows = computed(() =>
    model.value.monsters
      .map((row, index) => ({ row, index }))
      .filter(({ row }) => `${row.monster_name} ${row.monster_id}`.includes(props.search.trim())),
  );
</script>
