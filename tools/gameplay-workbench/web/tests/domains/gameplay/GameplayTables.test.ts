import { flushPromises, mount } from "@vue/test-utils";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { createPinia, setActivePinia } from "pinia";
import FilterCombobox from "@/components/FilterCombobox.vue";

import GameplayBackupPanel from "@/domains/gameplay/GameplayBackupPanel.vue";
import MapSpawnEditor from "@/domains/gameplay/MapSpawnEditor.vue";
import MonsterDropEditor from "@/domains/gameplay/MonsterDropEditor.vue";
import MigrationRecordsTable from "@/domains/gameplay/MigrationRecordsTable.vue";
import { gameplayApi, type Drops, type Spawns } from "@/services/gameplayApi";

beforeEach(() => setActivePinia(createPinia()));
afterEach(() => vi.restoreAllMocks());

describe("gameplay tables", () => {
  it("edits the original drop row and keeps field errors after searching", async () => {
    const data: Drops = {
      schema_version: 1,
      monster_id: 1,
      monster_name: "鹿",
      items: [
        { item_id: 1, item_name: "金币", group: 0, prob_recip: 1, repeat: 1, count: 2 },
        { item_id: 2, item_name: "木剑", group: 1, prob_recip: 10, repeat: 1, count: 3 },
      ],
    };
    const wrapper = mount(MonsterDropEditor, {
      props: {
        modelValue: [
          data,
          {
            ...data,
            monster_id: 2,
            monster_name: "鸡",
            items: [
              { item_id: 3, item_name: "鸡肉", group: 0, prob_recip: 1, repeat: 1, count: 1 },
            ],
          },
        ],
        errors: { "1.items.1.count": "数量错误" },
      },
    });
    await wrapper.get('input[type="search"]').setValue("木剑");
    expect(wrapper.findAll("tbody tr")).toHaveLength(1);
    expect(wrapper.text()).toContain("数量错误");
    await wrapper.get('input[aria-label="第 2 项 数量"]').setValue("9");
    expect(data.items.map((item) => item.count)).toEqual([2, 9]);
    expect(wrapper.emitted("edit")).toEqual([["1.items.1.count"]]);
    await wrapper.get('button[aria-label="清除搜索掉落物品"]').trigger("click");
    expect(wrapper.findAll("tbody tr")).toHaveLength(3);
    const filter = (label: string) =>
      wrapper
        .findAllComponents(FilterCombobox)
        .find((component) => component.props("label") === label)!;
    filter("怪物").vm.$emit("update:modelValue", 1);
    await flushPromises();
    filter("物品").vm.$emit("update:modelValue", 1);
    await flushPromises();
    filter("组").vm.$emit("update:modelValue", 0);
    await flushPromises();
    expect(wrapper.findAll("tbody tr")).toHaveLength(1);
    expect(wrapper.get("tbody tr").text()).toContain("金币");
    filter("怪物").vm.$emit("update:modelValue", null);
    await flushPromises();
    expect(wrapper.findAll("tbody tr")).toHaveLength(3);
    expect(data.items[1].count).toBe(9);
    wrapper.unmount();
  });

  it("keeps group and region indexes when editing a filtered spawn table", async () => {
    const region = { x: 1, y: 2, w: 3, h: 4, count: 5, time: 60, cratio: 10 };
    const data: Spawns = {
      schema_version: 1,
      map_id: 1,
      map_name: "道馆",
      groups: [
        { monster_id: 1, monster_name: "鸡", loc: [{ ...region }] },
        { monster_id: 2, monster_name: "鹿", loc: [{ ...region }, { ...region }] },
      ],
    };
    const wrapper = mount(MapSpawnEditor, {
      props: {
        modelValue: [
          data,
          {
            ...data,
            map_id: 2,
            map_name: "比奇",
            groups: [{ monster_id: 3, monster_name: "羊", loc: [{ ...region }] }],
          },
        ],
        errors: { "1.groups.1.loc.1.count": "目标数错误" },
      },
    });
    await wrapper.get('input[type="search"]').setValue("鹿");
    expect(wrapper.findAll("table")).toHaveLength(1);
    expect(wrapper.findAll("tbody tr")).toHaveLength(2);
    expect(wrapper.text()).toContain("目标数错误");
    const filter = (label: string) =>
      wrapper
        .findAllComponents(FilterCombobox)
        .find((component) => component.props("label") === label)!;
    filter("地图").vm.$emit("update:modelValue", 1);
    await flushPromises();
    filter("怪物").vm.$emit("update:modelValue", 2);
    await flushPromises();
    filter("组").vm.$emit("update:modelValue", 1);
    await flushPromises();
    filter("区域").vm.$emit("update:modelValue", 1);
    await flushPromises();
    expect(wrapper.findAll("tbody tr")).toHaveLength(1);
    await wrapper.get('input[aria-label="鹿 区域 2 目标数"]').setValue("0");
    expect(data.groups.map((group) => group.loc.map((entry) => entry.count))).toEqual([
      [5],
      [5, 0],
    ]);
    expect(wrapper.emitted("edit")).toEqual([["1.groups.1.loc.1.count"]]);
    await wrapper.get('input[type="search"]').setValue("");
    filter("怪物").vm.$emit("update:modelValue", 1);
    await flushPromises();
    expect(filter("组").props("modelValue")).toBeNull();
    expect(filter("区域").props("modelValue")).toBeNull();
    expect(wrapper.get("tbody tr").text()).toContain("鸡");
    filter("地图").vm.$emit("update:modelValue", null);
    await flushPromises();
    expect(wrapper.findAll("tbody tr")).toHaveLength(4);
    wrapper.unmount();
  });

  it("searches backups without enabling restore for another directory", async () => {
    const backup = {
      backup_id: "local",
      name: "2026-11-07 08:00:00",
      created_at: "2026-11-07T08:00:00+08:00",
      description: "调整前",
      available: true,
      source_server_root: "server",
      source_matches: true,
    };
    vi.spyOn(gameplayApi, "backups").mockResolvedValue([
      backup,
      { ...backup, backup_id: "other", description: "其他存档", source_matches: false },
    ]);
    const wrapper = mount(GameplayBackupPanel, {
      props: { workspaceId: "workspace", disabled: false, hasUnsavedDrafts: false },
    });
    await flushPromises();
    await wrapper.get('input[type="search"]').setValue("其他存档");
    const row = wrapper.get("tbody tr");
    expect(row.text()).toContain("其他目录");
    expect(row.get("button").attributes("disabled")).toBeDefined();
    await wrapper.get('input[type="search"]').setValue("调整前");
    expect(wrapper.get("tbody tr button").attributes("disabled")).toBeUndefined();
    wrapper.unmount();
  });

  it("finds migration records by their original content", async () => {
    const wrapper = mount(MigrationRecordsTable, {
      props: {
        records: [
          {
            domain: "drops",
            source_path: "drops.inc",
            source_position: 1,
            reason: "未生效",
            raw: { item_name: "木剑" },
          },
          {
            domain: "spawns",
            source_path: "spawns.inc",
            source_position: 2,
            reason: "未生效",
            raw: { monster_name: "鹿" },
          },
        ],
      },
    });
    await wrapper.get('input[type="search"]').setValue("木剑");
    expect(wrapper.findAll("tbody tr")).toHaveLength(1);
    expect(wrapper.get("tbody tr").text()).toContain("drops.inc");
    expect(wrapper.get("tbody tr button").text()).toBe("查看详情");
    await wrapper.get('input[type="search"]').setValue("不存在");
    expect(wrapper.text()).toContain("没有匹配的迁移记录");
    wrapper.unmount();
  });
});
