import { flushPromises, mount } from "@vue/test-utils";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import IntegerField from "@/components/IntegerField.vue";
import { useGameplay } from "@/composables/useGameplay";
import GameplayPage from "@/domains/gameplay/GameplayPage.vue";
import { ApiClientError } from "@/services/api";
import {
  gameplayApi,
  validateNumbers,
  type Catalog,
  type ConfigFile,
  type Domain,
} from "@/services/gameplayApi";

function file(domain: Domain = "monsters", id: number | null = null): ConfigFile {
  return {
    workspace_id: "workspace",
    catalog_revision: "catalog",
    revision: "old-revision",
    domain,
    object_id: id,
    data:
      domain === "monsters"
        ? { schema_version: 1, monsters: [{ monster_id: 1, monster_name: "鹿", exp: 10 }] }
        : domain === "drops"
          ? {
              schema_version: 1,
              monster_id: 1,
              monster_name: "鹿",
              items: [
                { item_id: 2, item_name: "金币", group: 0, prob_recip: 1, repeat: 1, count: 2 },
              ],
            }
          : { schema_version: 1, map_id: 3, map_name: "道馆", groups: [] },
  };
}

const catalog: Catalog = {
  workspace_id: "workspace",
  catalog_revision: "catalog",
  catalog: {
    monsters: [{ id: 1, name: "鹿" }],
    items: [{ id: 2, name: "金币" }],
    maps: [{ id: 3, name: "道馆" }],
  },
  manifest: { schema_version: 1, drop_monsters: [1], spawn_maps: [3] },
  migration_notes: { records: [] },
  fields: {},
};

beforeEach(() => {
  vi.spyOn(gameplayApi, "workspace").mockResolvedValue({
    bound: true,
    workspace_id: "workspace",
    server_root: "server",
  });
  vi.spyOn(gameplayApi, "catalog").mockResolvedValue(catalog);
  vi.spyOn(gameplayApi, "read").mockImplementation(async (_workspace, domain, id) =>
    file(domain, id),
  );
  vi.spyOn(gameplayApi, "validate").mockResolvedValue({ valid: true, issues: [] });
  vi.spyOn(gameplayApi, "save").mockImplementation(async (payload) => ({
    ...payload,
    revision: "new-revision",
  }));
});
afterEach(() => vi.restoreAllMocks());

describe("gameplay editing", () => {
  it("keeps empty and out-of-range inputs as field errors without sending", async () => {
    const wrapper = mount(GameplayPage);
    await flushPromises();
    await wrapper.get('input[aria-label="鹿 基础经验"]').setValue("");
    await wrapper
      .findAll("button")
      .find((button) => button.text() === "校验并保存当前文件")!
      .trigger("click");
    await flushPromises();
    expect(wrapper.get('input[aria-label="鹿 基础经验"]').attributes("aria-invalid")).toBe("true");
    expect(wrapper.find(".field-error").text()).toContain("整数");
    expect(gameplayApi.save).not.toHaveBeenCalled();
    await wrapper.get('input[aria-label="鹿 基础经验"]').setValue("20");
    expect(wrapper.get('input[aria-label="鹿 基础经验"]').attributes("aria-invalid")).toBe("false");
    wrapper.unmount();
  });

  it("keeps per-file drafts and preserves a conflict without replacing the baseline", async () => {
    const state = useGameplay();
    await state.refreshWorkspace();
    if (!state.draft.value || !("monsters" in state.draft.value))
      throw new Error("missing fixture");
    state.draft.value.monsters[0].exp = 25;
    await state.load("drops", 1);
    await state.load("monsters");
    expect(state.draft.value).toEqual({
      ...file().data,
      monsters: [{ monster_id: 1, monster_name: "鹿", exp: 25 }],
    });
    vi.mocked(gameplayApi.save).mockRejectedValueOnce(
      new ApiClientError("File changed", "http_error", { status: 409 }),
    );
    await state.save();
    expect(state.conflict.value).toBe(true);
    expect(state.dirty.value).toBe(true);
    expect(state.base.value?.revision).toBe("old-revision");
    expect(state.message.value).toContain("草稿已保留");
  });

  it("shows a successful save and uses the returned file revision", async () => {
    const state = useGameplay();
    await state.refreshWorkspace();
    if (!state.draft.value || !("monsters" in state.draft.value))
      throw new Error("missing fixture");
    state.draft.value.monsters[0].exp = 30;
    await state.save();
    expect(state.dirty.value).toBe(false);
    expect(state.base.value?.revision).toBe("new-revision");
    expect(state.message.value).toContain("下次启动");
  });

  it("ignores an earlier read response when another object has already been selected", async () => {
    const state = useGameplay();
    await state.refreshWorkspace();
    let finish!: (value: ConfigFile) => void;
    vi.mocked(gameplayApi.read).mockImplementationOnce(
      () =>
        new Promise((resolve) => {
          finish = resolve;
        }),
    );
    const pending = state.load("drops", 1);
    await state.load("spawns", 3);
    finish(file("drops", 1));
    await pending;
    expect(state.domain.value).toBe("spawns");
    expect(state.objectId.value).toBe(3);
  });

  it("checks integer constraints without converting invalid values", () => {
    const data = file("drops", 1).data;
    if (!("items" in data)) throw new Error("missing fixture");
    data.items[0].prob_recip = 0;
    data.items[0].count = 1.5;
    expect(Object.keys(validateNumbers(data))).toEqual(["items.0.prob_recip", "items.0.count"]);
  });

  it("forgets a cached draft after the user restores its original values", async () => {
    const state = useGameplay();
    await state.refreshWorkspace();
    if (!state.draft.value || !("monsters" in state.draft.value))
      throw new Error("missing fixture");
    state.draft.value.monsters[0].exp = 25;
    await state.load("drops", 1);
    await state.load("monsters");
    if (!state.draft.value || !("monsters" in state.draft.value))
      throw new Error("missing fixture");
    state.draft.value.monsters[0].exp = 10;
    await state.load("drops", 1);
    await state.load("monsters");
    expect(state.dirty.value).toBe(false);
    expect(state.draft.value).toEqual(file().data);
  });

  it("ignores a late save after the user selects a different file", async () => {
    const state = useGameplay();
    await state.refreshWorkspace();
    let finish!: (value: ConfigFile) => void;
    vi.mocked(gameplayApi.save).mockImplementationOnce(
      () =>
        new Promise((resolve) => {
          finish = resolve;
        }),
    );
    const pending = state.save();
    await flushPromises();
    await state.load("spawns", 3);
    finish({ ...file(), revision: "late-save" });
    await pending;
    expect(state.domain.value).toBe("spawns");
    expect(state.base.value?.revision).toBe("old-revision");
  });

  it("does not apply an obsolete workspace response", async () => {
    const state = useGameplay();
    let finish!: (value: Awaited<ReturnType<typeof gameplayApi.workspace>>) => void;
    vi.mocked(gameplayApi.workspace).mockImplementationOnce(
      () =>
        new Promise((resolve) => {
          finish = resolve;
        }),
    );
    const pending = state.refreshWorkspace();
    await state.refreshWorkspace();
    finish({ bound: true, workspace_id: "obsolete", server_root: "other" });
    await pending;
    expect(state.workspace.value?.workspace_id).toBe("workspace");
  });

  it("associates an integer error with its real input", () => {
    const wrapper = mount(IntegerField, {
      props: { modelValue: -1, path: "items.0.count", label: "数量", error: "请输入正整数" },
    });
    const input = wrapper.get("input");
    expect(input.attributes("aria-describedby")).toBe("items.0.count-error");
    expect(wrapper.get("#items\\.0\\.count-error").text()).toBe("请输入正整数");
    wrapper.unmount();
  });
});
