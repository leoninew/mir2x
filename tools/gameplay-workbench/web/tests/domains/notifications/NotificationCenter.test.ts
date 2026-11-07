import { fireEvent, render, screen } from "@testing-library/vue";
import { createPinia } from "pinia";
import { defineComponent, h } from "vue";
import { afterEach, beforeEach, describe, expect, it } from "vitest";

import NotificationCenter from "@/domains/notifications/NotificationCenter.vue";

beforeEach(() => window.localStorage.clear());
afterEach(() => window.localStorage.clear());

describe("NotificationCenter", () => {
  it("combines search and status filtering and marks a matching notification read", async () => {
    const { unmount } = render(NotificationCenter, {
      global: {
        plugins: [createPinia()],
        stubs: {
          FilterCombobox: defineComponent({
            props: {
              modelValue: { type: [String, Number], default: null },
              label: { type: String, required: true },
              options: { type: Array, default: () => [] },
            },
            emits: ["update:modelValue"],
            setup(props, { emit }) {
              return () =>
                h(
                  "button",
                  { "aria-label": props.label, onClick: () => emit("update:modelValue", "unread") },
                  "Unread",
                );
            },
          }),
        },
      },
    });

    expect(screen.getByText("Deploy finished")).toBeTruthy();
    expect(screen.getByText("Review requested")).toBeTruthy();

    const search = screen.getByRole("searchbox", { name: "Search notifications" });
    await fireEvent.update(search, "review");
    expect(screen.queryByText("Deploy finished")).toBeNull();
    expect(screen.getByText("Review requested")).toBeTruthy();
    await fireEvent.click(screen.getByRole("button", { name: "Notification filter" }));
    expect(screen.getByText("No matching notifications.")).toBeTruthy();
    await fireEvent.update(search, "");
    expect(screen.getByText("Deploy finished")).toBeTruthy();
    expect(screen.queryByText("Review requested")).toBeNull();
    await fireEvent.click(screen.getByRole("button", { name: "Mark read" }));
    expect(screen.getByText("No unread notifications.")).toBeTruthy();
    unmount();
  });
});
