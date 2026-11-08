<template>
  <ProgressRoot
    as="span"
    :model-value="null"
    :aria-label="label"
    :class="[
      'items-center justify-center gap-2 text-sm',
      compact ? 'text-current' : 'text-slate-500',
      overlay ? 'absolute inset-0 z-30 flex rounded-[inherit] bg-white/80' : 'inline-flex',
    ]"
  >
    <ProgressIndicator as-child>
      <LoaderCircle
        class="size-5 shrink-0 animate-spin motion-reduce:animate-none"
        aria-hidden="true"
      />
    </ProgressIndicator>
    <span :class="compact ? 'sr-only' : ''">{{ label }}</span>
  </ProgressRoot>
</template>

<script setup lang="ts">
  import { LoaderCircle } from "@lucide/vue";
  import { ProgressRoot, ProgressIndicator } from "reka-ui";

  withDefaults(defineProps<{ label?: string; overlay?: boolean; compact?: boolean }>(), {
    label: "加载中…",
    overlay: false,
    compact: false,
  });
</script>
