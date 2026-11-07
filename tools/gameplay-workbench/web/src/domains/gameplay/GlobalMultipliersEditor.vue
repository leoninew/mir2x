<template>
  <div class="flex min-h-0 flex-1 flex-col gap-3">
    <div class="table-toolbar">
      <span class="text-sm text-slate-600">独立倍率配置</span>
      <div class="ml-auto"><slot name="actions"></slot></div>
    </div>
    <section class="min-h-0 overflow-auto rounded-lg border border-slate-200 bg-white p-5 sm:p-6">
      <div class="grid gap-5 lg:grid-cols-3">
        <div
          v-for="field in multiplierFields"
          :key="field.key"
          class="rounded-lg border border-slate-200 bg-slate-50 p-5"
        >
          <MultiplierField
            v-model="model[field.key]"
            :path="field.key"
            :label="field.label"
            :error="errors[field.key]"
            @edit="$emit('edit', $event)"
          />
          <p class="mt-3 text-xs leading-5 text-slate-500">{{ field.help }}</p>
        </div>
      </div>
    </section>
  </div>
</template>

<script setup lang="ts">
  import MultiplierField from "@/components/MultiplierField.vue";
  import type { Multipliers } from "@/services/gameplayApi";
  const model = defineModel<Multipliers>({ required: true });
  defineProps<{ errors: Record<string, string> }>();
  defineEmits<{ edit: [path: string] }>();
  const multiplierFields = [
    { key: "experience", label: "经验倍率", help: "乘以原击杀经验结果；0 关闭击杀经验。" },
    { key: "gold", label: "金币倍率", help: "与原金币倍率相乘；0 关闭金币掉落。" },
    { key: "drop", label: "掉率倍率", help: "与原掉率倍率相乘；0 关闭全部掉落。" },
  ] as const;
</script>
