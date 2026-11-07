<template>
  <section class="p-5 sm:p-6">
    <p class="max-w-3xl text-sm leading-6 text-slate-600">
      独立保存，默认均为 1，支持小数。金币与掉率会乘以服务端已有倍率；经验乘在原击杀经验结果上。0
      分别关闭击杀经验、金币掉落或全部掉落。
    </p>
    <div class="mt-6 grid gap-5 lg:grid-cols-3">
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
