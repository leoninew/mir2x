<template>
  <div>
    <input
      :id="path"
      :value="raw"
      :aria-label="label"
      :aria-invalid="Boolean(error)"
      :aria-describedby="error ? `${path}-error` : undefined"
      :class="['number-field', { 'number-field-invalid': error }]"
      type="number"
      step="1"
      :min="minimum"
      max="2147483647"
      required
      @input="update"
    />
    <p v-if="error" :id="`${path}-error`" class="field-error" role="alert">{{ error }}</p>
  </div>
</template>

<script setup lang="ts">
  import { ref, watch } from "vue";

  const props = withDefaults(
    defineProps<{
      modelValue: number;
      path: string;
      label: string;
      error?: string;
      minimum?: number;
    }>(),
    { error: undefined, minimum: 0 },
  );
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

<style scoped>
  .number-field {
    width: 100%;
    min-width: 5.5rem;
    border: 1px solid var(--color-slate-300);
    border-radius: 0.375rem;
    padding: 0.45rem 0.6rem;
    background: white;
    font-variant-numeric: tabular-nums;
  }
  .number-field:focus-visible {
    outline: 2px solid var(--color-slate-600);
    outline-offset: 2px;
  }
  .number-field-invalid {
    border-color: var(--color-red-600);
  }
  .field-error {
    margin-top: 0.25rem;
    color: var(--color-red-700);
    font-size: 0.75rem;
    font-weight: 400;
    line-height: 1.5;
  }
</style>
