<template>
  <div class="grid gap-1.5">
    <label :for="path" class="text-sm text-slate-700">
      {{ label }} <span class="text-red-600">*</span>
    </label>
    <input
      :id="path"
      :value="raw"
      :aria-invalid="Boolean(error)"
      :aria-describedby="error ? `${path}-error` : undefined"
      :class="[
        'rounded-md border bg-white px-3 py-2 tabular-nums',
        error ? 'border-red-600' : 'border-slate-300',
      ]"
      type="number"
      step="any"
      min="0"
      required
      @input="update"
    />
    <p v-if="error" :id="`${path}-error`" class="text-xs text-red-700" role="alert">
      {{ error }}
    </p>
  </div>
</template>

<script setup lang="ts">
  import { ref, watch } from "vue";

  const props = defineProps<{
    modelValue: number;
    path: string;
    label: string;
    error?: string;
  }>();
  const emit = defineEmits<{ "update:modelValue": [value: number]; edit: [path: string] }>();
  const raw = ref(Number.isFinite(props.modelValue) ? String(props.modelValue) : "");
  watch(
    () => props.modelValue,
    (value) => {
      if (Number.isFinite(value) && (raw.value === "" || Number(raw.value) !== value))
        raw.value = String(value);
    },
  );
  function update(event: Event): void {
    const input = event.target as HTMLInputElement;
    raw.value = input.value;
    emit("update:modelValue", input.valueAsNumber);
    emit("edit", props.path);
  }
</script>
