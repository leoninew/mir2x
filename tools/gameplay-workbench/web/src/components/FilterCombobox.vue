<template>
  <ComboboxRoot
    v-model="model"
    :disabled="disabled"
    reset-model-value-on-clear
    open-on-click
    class="shrink-0"
    :class="$attrs.class ? undefined : 'w-56'"
  >
    <ComboboxAnchor
      class="flex h-9 items-center rounded-md border border-slate-300 bg-white focus-within:outline-2 focus-within:outline-slate-600"
    >
      <ComboboxInput
        v-model="query"
        :display-value="displayValue"
        :aria-label="label"
        :placeholder="label"
        :title="displayValue(model)"
        class="min-w-0 flex-1 bg-transparent pl-3 text-sm text-slate-800 outline-none placeholder:text-slate-400 disabled:opacity-50"
      />
      <ComboboxCancel
        v-if="model !== null || query"
        :aria-label="`清除${label}`"
        :disabled="disabled"
        class="filter-control"
        tabindex="0"
      >
        <X class="size-4" aria-hidden="true" />
      </ComboboxCancel>
      <ComboboxTrigger :aria-label="`展开${label}`" :disabled="disabled" class="filter-control">
        <ChevronDown class="size-4" aria-hidden="true" />
      </ComboboxTrigger>
    </ComboboxAnchor>
    <ComboboxPortal>
      <ComboboxContent
        position="popper"
        :side-offset="4"
        class="z-50 max-h-72 min-w-[var(--reka-combobox-trigger-width)] overflow-auto rounded-md border border-slate-200 bg-white p-1 shadow-lg"
      >
        <ComboboxViewport>
          <ComboboxEmpty class="px-3 py-2 text-sm text-slate-500">没有匹配选项</ComboboxEmpty>
          <ComboboxItem
            v-for="option in options"
            :key="option.value"
            :value="option.value"
            :text-value="option.label"
            class="cursor-default rounded px-3 py-2 text-sm text-slate-700 outline-none data-[highlighted]:bg-indigo-50 data-[state=checked]:text-indigo-700"
          >
            {{ option.label }}
          </ComboboxItem>
        </ComboboxViewport>
      </ComboboxContent>
    </ComboboxPortal>
  </ComboboxRoot>
</template>

<script setup lang="ts">
  import { ChevronDown, X } from "@lucide/vue";
  import { ref } from "vue";
  import {
    ComboboxRoot,
    ComboboxAnchor,
    ComboboxInput,
    ComboboxCancel,
    ComboboxTrigger,
    ComboboxPortal,
    ComboboxContent,
    ComboboxViewport,
    ComboboxEmpty,
    ComboboxItem,
  } from "reka-ui";

  const model = defineModel<string | number | null>({ default: null });
  const props = defineProps<{
    label: string;
    options: { value: string | number; label: string }[];
    disabled?: boolean;
  }>();
  const query = ref("");
  function displayValue(value: string | number | null): string {
    return props.options.find((option) => option.value === value)?.label ?? "";
  }
</script>
