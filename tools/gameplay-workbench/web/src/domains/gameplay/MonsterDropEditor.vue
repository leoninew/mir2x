<template>
  <div>
    <p class="editor-help">{{ fields.prob_recip }}。{{ fields.group }}。</p>
    <table class="config-table">
      <thead>
        <tr>
          <th>原始顺序</th>
          <th>物品</th>
          <th v-for="field in dropFields" :key="field.key">
            {{ field.label }} <span class="required">*</span>
          </th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="(row, index) in model.items" :key="index">
          <td class="text-slate-500">{{ index + 1 }}</td>
          <td>
            {{ row.item_name
            }}<span class="block text-xs text-slate-500">ID {{ row.item_id }}</span>
          </td>
          <td v-for="field in dropFields" :key="field.key">
            <IntegerField
              v-model="row[field.key]"
              :path="`items.${index}.${field.key}`"
              :label="`第 ${index + 1} 项 ${field.label}`"
              :minimum="field.key === 'group' ? 0 : 1"
              :error="errors[`items.${index}.${field.key}`]"
              @edit="$emit('edit', $event)"
            />
          </td>
        </tr>
        <tr v-if="!model.items.length">
          <td colspan="7">此怪物没有配置掉落条目。</td>
        </tr>
      </tbody>
    </table>
  </div>
</template>

<script setup lang="ts">
  import IntegerField from "@/components/IntegerField.vue";
  import type { Drops } from "@/services/gameplayApi";
  const model = defineModel<Drops>({ required: true });
  defineProps<{ errors: Record<string, string>; fields: Record<string, string> }>();
  defineEmits<{ edit: [path: string] }>();
  const dropFields = [
    { key: "group", label: "组" },
    { key: "prob_recip", label: "概率分母" },
    { key: "repeat", label: "尝试次数" },
    { key: "count", label: "数量" },
  ] as const;
</script>
