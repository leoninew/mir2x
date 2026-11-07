import { mount, flushPromises } from "@vue/test-utils";
import { describe, expect, it } from "vitest";
import { ComboboxRoot } from "reka-ui";
import FilterCombobox from "@/components/FilterCombobox.vue";

describe("FilterCombobox", () => {
  it("starts empty and clears a zero-valued selection to null", async () => {
    const wrapper = mount(FilterCombobox, {
      props: {
        label: "组",
        modelValue: null,
        options: [{ value: 0, label: "组 0" }],
        "onUpdate:modelValue": (value) => wrapper.setProps({ modelValue: value }),
      },
    });
    expect((wrapper.get("input").element as HTMLInputElement).value).toBe("");
    wrapper.getComponent(ComboboxRoot).vm.$emit("update:modelValue", 0);
    await flushPromises();
    expect((wrapper.get("input").element as HTMLInputElement).value).toBe("组 0");
    await wrapper.get('button[aria-label="清除组"]').trigger("click");
    await flushPromises();
    expect(wrapper.props("modelValue")).toBeNull();
    expect((wrapper.get("input").element as HTMLInputElement).value).toBe("");
    wrapper.unmount();
  });
});
