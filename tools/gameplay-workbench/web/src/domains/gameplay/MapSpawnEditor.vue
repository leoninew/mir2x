<template>
  <div>
    <p class="editor-help">
      周期单位为秒；目标数为 0 时仍沿原逻辑至少生成 1 个。cratio 当前未消费，只读保留。
    </p>
    <section
      v-for="(group, groupIndex) in model.groups"
      :key="groupIndex"
      class="border-b border-slate-200"
    >
      <h2 class="bg-slate-100/70 px-4 py-3 text-sm font-medium">
        {{ groupIndex + 1 }} · {{ group.monster_name }}
        <span class="font-normal text-slate-500">ID {{ group.monster_id }}</span>
      </h2>
      <table class="config-table">
        <thead>
          <tr>
            <th v-for="field in spawnFields" :key="field.key">
              {{ field.label }} <span class="required">*</span>
            </th>
            <th>cratio · 未使用</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="(region, regionIndex) in group.loc" :key="regionIndex">
            <td v-for="field in spawnFields" :key="field.key">
              <IntegerField
                v-model="region[field.key]"
                :path="`groups.${groupIndex}.loc.${regionIndex}.${field.key}`"
                :label="`${group.monster_name} 区域 ${regionIndex + 1} ${field.label}`"
                :minimum="field.minimum"
                :error="errors[`groups.${groupIndex}.loc.${regionIndex}.${field.key}`]"
                @edit="$emit('edit', $event)"
              />
            </td>
            <td class="text-slate-500">{{ region.cratio ?? "—" }}</td>
          </tr>
        </tbody>
      </table>
    </section>
    <p v-if="!model.groups.length" class="p-5 text-sm text-slate-500">此地图没有有效刷怪组。</p>
  </div>
</template>

<script setup lang="ts">
  import IntegerField from "@/components/IntegerField.vue";
  import type { Spawns } from "@/services/gameplayApi";
  const model = defineModel<Spawns>({ required: true });
  defineProps<{ errors: Record<string, string> }>();
  defineEmits<{ edit: [path: string] }>();
  const spawnFields = [
    { key: "x", label: "X", minimum: -2147483648 },
    { key: "y", label: "Y", minimum: -2147483648 },
    { key: "w", label: "宽度", minimum: 1 },
    { key: "h", label: "高度", minimum: 1 },
    { key: "count", label: "目标数", minimum: 0 },
    { key: "time", label: "周期（秒）", minimum: 1 },
  ] as const;
</script>
