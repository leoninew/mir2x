import { createPinia, setActivePinia } from "pinia";
import { beforeEach, describe, expect, it } from "vitest";
import { useToastsStore } from "@/stores/toasts";

beforeEach(() => setActivePinia(createPinia()));

describe("toasts", () => {
  it("refreshes repeated notices and dismisses the requested notice", () => {
    const toasts = useToastsStore();
    toasts.show("保存失败", "error");
    const previous = toasts.items[0].id;
    toasts.show("保存失败", "error");
    toasts.show("路径已保存", "success");
    expect(toasts.items).toHaveLength(2);
    expect(toasts.items[0]).toMatchObject({ tone: "error", duration: 8000 });
    expect(toasts.items[0].id).not.toBe(previous);
    toasts.dismiss(toasts.items[0].id);
    expect(toasts.items.map((item) => item.message)).toEqual(["路径已保存"]);
  });

  it("limits visible notices and ignores empty messages", () => {
    const toasts = useToastsStore();
    toasts.show(" ");
    for (let index = 0; index < 6; index++) toasts.show(`提示 ${index}`);
    expect(toasts.items).toHaveLength(5);
    expect(toasts.items[0].message).toBe("提示 1");
  });
});
