export type GMEntry = "chat" | "client-lua" | "server-local" | "server-async";

export type GMCommand = {
  id: string;
  name: string;
  entry: GMEntry;
  category: string;
  description: string;
  syntax: string[];
  parameters?: { name: string; type: string; description: string }[];
  examples: { label: string; code: string; template?: boolean }[];
  notes?: string[];
  source: string;
  keywords?: string[];
};

export const GM_ENTRIES: Record<GMEntry, { label: string; prefix: string; usage: string }> = {
  chat: {
    label: "聊天命令",
    prefix: "@",
    usage: "在客户端聊天框输入并提交。命令名区分大小写，支持唯一前缀缩写；参数以空格分隔。",
  },
  "client-lua": {
    label: "客户端 Lua",
    prefix: "$",
    usage: "在客户端聊天框输入 Lua 代码。该环境操作当前客户端与本地角色表现。",
  },
  "server-local": {
    label: "服务端 Lua · LOCAL",
    prefix: "LOCAL",
    usage: "服务端窗口选择 Command → Run，再选 LOCAL。输入 Lua，不加 $。",
  },
  "server-async": {
    label: "服务端 Lua · ASYNC",
    prefix: "ASYNC",
    usage:
      "服务端窗口选择 Command → Run，再选 ASYNC。命令经 actor 执行，可对在线角色或活动世界对象进行服务端操作。",
  },
};

export const GM_CATEGORIES = [
  "移动",
  "角色状态",
  "经验等级",
  "金币物品",
  "宠物与世界",
  "查询与工具",
] as const;

export const GM_COMMANDS: GMCommand[] = [
  {
    id: "chat-help",
    name: "help",
    entry: "chat",
    category: "查询与工具",
    description: "列出当前客户端注册的聊天命令。",
    syntax: ["@help"],
    examples: [{ label: "查看命令列表", code: "@help" }],
    source: "client/src/processrun.cpp · RegisterUserCommand()",
    keywords: ["帮助", "列表"],
  },
  {
    id: "chat-move",
    name: "moveTo",
    entry: "chat",
    category: "移动",
    description: "传送到当前地图或指定基础地图的随机可通行位置，也可指定坐标。",
    syntax: ["@moveTo", "@moveTo x y", "@moveTo mapID", "@moveTo mapID x y"],
    parameters: [
      { name: "mapID", type: "整数，可选", description: "地图记录 ID；省略时留在当前地图。" },
      { name: "x y", type: "整数，可选", description: "坐标；须成对提供且须是可通行点。" },
    ],
    examples: [
      { label: "比奇地图随机落点", code: "@moveTo 1" },
      { label: "比奇地图指定坐标", code: "@moveTo 1 414 349" },
    ],
    notes: ["地图 ID 1 的记录名是“比奇县_0”。这是传送请求，不是寻路行走。"],
    source: "client/src/processrun.cpp · RegisterUserCommand() / registerLuaExport()",
    keywords: ["比奇", "比奇县_0", "传送", "坐标"],
  },
  {
    id: "chat-make-item",
    name: "makeItem",
    entry: "chat",
    category: "金币物品",
    description: "请求服务端生成指定物品并加入当前角色的库存。",
    syntax: ["@makeItem 物品名 数量"],
    parameters: [
      { name: "物品名", type: "字符串", description: "须匹配物品表名称。" },
      { name: "数量", type: "整数 > 1", description: "当前聊天命令实际上只接受大于 1。" },
    ],
    examples: [{ label: "生成木剑", code: "@makeItem 木剑 2" }],
    notes: [
      "不要用于增加金币余额：聊天命令走物品库存写入，不是角色金币字段。",
      "当前默认数量 1 的分支会访问缺失参数；显式数量 1 也会被拒绝。单个普通物品可用客户端 Lua makeItem。",
    ],
    source:
      "client/src/processrun.cpp · RegisterUserCommand()；server/src/playernet.cpp · net_CM_MAKEITEM()",
    keywords: ["造物", "背包", "库存", "金币", "缺陷"],
  },
  {
    id: "chat-attack-uid",
    name: "getAttackUID",
    entry: "chat",
    category: "查询与工具",
    description: "输出当前攻击焦点对象的 UID。",
    syntax: ["@getAttackUID"],
    examples: [{ label: "查看攻击焦点", code: "@getAttackUID" }],
    source: "client/src/processrun.cpp · RegisterUserCommand()",
    keywords: ["目标", "UID", "焦点"],
  },
  {
    id: "chat-add-hp",
    name: "addHP",
    entry: "chat",
    category: "角色状态",
    description: "请求增加当前角色 HP；死亡角色增加正 HP 时会原地复活。",
    syntax: ["@addHP 数量"],
    parameters: [{ name: "数量", type: "整数", description: "服务器将负值截为 0。" }],
    examples: [{ label: "增加 100 HP", code: "@addHP 100" }],
    notes: ["这是调试加血，不同于正常死亡后的回城复活。"],
    source:
      "client/src/processrun.cpp · RegisterUserCommand()；server/src/playernet.cpp · net_CM_REQUESTADDHEALTH()",
    keywords: ["血量", "生命", "复活"],
  },
  {
    id: "chat-add-mp",
    name: "addMP",
    entry: "chat",
    category: "角色状态",
    description: "请求增加当前角色 MP。",
    syntax: ["@addMP 数量"],
    parameters: [{ name: "数量", type: "整数", description: "服务器将负值截为 0。" }],
    examples: [{ label: "增加 100 MP", code: "@addMP 100" }],
    notes: ["只增加 MP 不会复活死亡角色。"],
    source:
      "client/src/processrun.cpp · RegisterUserCommand()；server/src/playernet.cpp · net_CM_REQUESTADDHEALTH()",
    keywords: ["魔法", "魔法值", "蓝量"],
  },
  {
    id: "chat-full-health",
    name: "xcg",
    entry: "chat",
    category: "角色状态",
    description: "按当前角色最大值恢复 HP 与 MP。",
    syntax: ["@xcg"],
    examples: [{ label: "恢复 HP 与 MP", code: "@xcg" }],
    source: "client/src/processrun.cpp · RegisterUserCommand()",
    keywords: ["满血", "满蓝", "恢复"],
  },
  {
    id: "chat-add-exp",
    name: "addExp",
    entry: "chat",
    category: "经验等级",
    description: "请求给当前角色增加经验值。",
    syntax: ["@addExp 数量"],
    parameters: [{ name: "数量", type: "整数", description: "请使用正常范围内的正整数。" }],
    examples: [{ label: "增加 1000 经验", code: "@addExp 1000" }],
    notes: ["参数转换没有完整错误处理，不要输入非数字或极大值。"],
    source:
      "client/src/processrun.cpp · RegisterUserCommand()；server/src/playernet.cpp · net_CM_REQUESTADDEXP()",
    keywords: ["升级", "经验"],
  },
  {
    id: "chat-add-level",
    name: "addLevel",
    entry: "chat",
    category: "经验等级",
    description: "按当前等级加上增量，计算目标等级并补足所需经验。",
    syntax: ["@addLevel 正整数"],
    parameters: [{ name: "增量", type: "正整数", description: "最终等级不得超过 1024。" }],
    examples: [{ label: "提升 1 级", code: "@addLevel 1" }],
    notes: ["实现方式是增加累计经验，不是直接写入等级字段。"],
    source:
      "client/src/processrun.cpp · RegisterUserCommand()；common/src/sysconst.hpp · SYS_MAXLEVEL",
    keywords: ["升级", "等级", "经验"],
  },
  {
    id: "chat-kill-pets",
    name: "killPets",
    entry: "chat",
    category: "宠物与世界",
    description: "请求杀死当前角色控制的全部宠物。",
    syntax: ["@killPets"],
    examples: [{ label: "结束全部宠物", code: "@killPets" }],
    source: "client/src/processrun.cpp · RegisterUserCommand()",
    keywords: ["宝宝", "宠物"],
  },
  {
    id: "chat-die",
    name: "die",
    entry: "chat",
    category: "角色状态",
    description: "请求当前角色死亡。",
    syntax: ["@die"],
    examples: [{ label: "请求角色死亡", code: "@die" }],
    notes: ["会立即改变角色状态。"],
    source:
      "client/src/processrun.cpp · RegisterUserCommand()；server/src/playernet.cpp · net_CM_REQUESTDIE()",
    keywords: ["自杀", "死亡"],
  },
  {
    id: "chat-lua-editor",
    name: "luaEditor",
    entry: "chat",
    category: "查询与工具",
    description: "保留的占位命令。",
    syntax: ["@luaEditor"],
    examples: [{ label: "查看当前状态", code: "@luaEditor" }],
    notes: ["Lua 编辑器尚未实现。"],
    source: "client/src/processrun.cpp · RegisterUserCommand()",
    keywords: ["Lua", "编辑器", "未实现"],
  },
  {
    id: "client-move",
    name: "moveTo",
    entry: "client-lua",
    category: "移动",
    description: "按地图名取得 ID，传送到随机落点或指定坐标。",
    syntax: ["moveTo()", "moveTo(x, y)", "moveTo(mapID)", "moveTo(mapID, x, y)"],
    parameters: [
      { name: "mapID", type: "整数，可选", description: "地图记录 ID。" },
      { name: "x, y", type: "整数，可选", description: "成对的目标坐标。" },
    ],
    examples: [
      { label: "移动到比奇地图随机落点", code: '$moveTo(getMapID("比奇县_0"))' },
      { label: "移动到比奇地图坐标", code: '$moveTo(getMapID("比奇县_0"), 414, 349)' },
    ],
    notes: ["传送请求，不会沿路径行走。"],
    source: "client/src/processrun.cpp · registerLuaExport()",
    keywords: ["比奇", "地图", "传送", "坐标"],
  },
  {
    id: "client-make-item",
    name: "makeItem",
    entry: "client-lua",
    category: "金币物品",
    description: "请求生成普通物品并加入当前角色库存。",
    syntax: ["makeItem(itemID 或物品名[, 数量])"],
    parameters: [
      {
        name: "itemID 或物品名",
        type: "整数或字符串",
        description: "数字按物品 ID，字符串按物品名。",
      },
      { name: "数量", type: "正整数，可选", description: "省略时默认为 1。" },
    ],
    examples: [{ label: "生成 1 把木剑", code: '$makeItem("木剑", 1)' }],
    notes: ["这是物品库存路径，不会给角色金币余额充值。"],
    source: "client/src/processrun.cpp · registerLuaExport()",
    keywords: ["造物", "背包", "库存", "金币"],
  },
  {
    id: "client-add-exp",
    name: "addExp",
    entry: "client-lua",
    category: "经验等级",
    description: "向当前角色发送增加经验请求。",
    syntax: ["addExp(经验数量)"],
    parameters: [{ name: "经验数量", type: "整数", description: "请使用正常范围内的正值。" }],
    examples: [{ label: "增加 1000 经验", code: "$addExp(1000)" }],
    source: "client/src/processrun.cpp · registerLuaExport()",
    keywords: ["升级", "经验"],
  },
  {
    id: "client-record-id",
    name: "getMapID / getItemID / getMonsterID",
    entry: "client-lua",
    category: "查询与工具",
    description: "按源码记录名称查询地图、物品或怪物 ID。",
    syntax: ["getMapID(地图名)", "getItemID(物品名)", "getMonsterID(怪物名)"],
    examples: [
      { label: "查询比奇地图 ID", code: '$addCBLog(CBLOG_SYS, tostring(getMapID("比奇县_0")))' },
    ],
    notes: ["使用与数据记录完全一致的名称。"],
    source: "common/src/luamodule.cpp · getMapID()/getItemID()/getMonsterID()",
    keywords: ["查 ID", "比奇", "物品", "怪物", "地图"],
  },
  {
    id: "client-player-list",
    name: "playerList",
    entry: "client-lua",
    category: "查询与工具",
    description: "返回当前客户端可见玩家的 UID 列表。",
    syntax: ["playerList()"],
    examples: [
      { label: "遍历可见玩家", code: "$for _, uid in ipairs(playerList()) do print(uid) end" },
    ],
    notes: ["只包含当前客户端可见玩家，不是全服在线列表。"],
    source: "client/src/processrun.cpp · registerLuaExport()/GetPlayerList()",
    keywords: ["玩家", "列表", "UID"],
  },
  {
    id: "client-appearance",
    name: "myHero_dress / myHero_weapon",
    entry: "client-lua",
    category: "金币物品",
    description: "修改当前客户端 Hero 的本地装备显示数据。",
    syntax: ["myHero_dress(itemID)", "myHero_weapon(itemID)"],
    examples: [
      { label: "设置本地衣服显示", code: "$myHero_dress(12345)", template: true },
      { label: "设置本地武器显示", code: "$myHero_weapon(12345)", template: true },
    ],
    notes: ["不会向服务器穿戴装备，也不会真正授予物品。"],
    source: "client/src/processrun.cpp · registerLuaExport()",
    keywords: ["外观", "装备", "衣服", "武器"],
  },
  {
    id: "client-log",
    name: "addCBLog",
    entry: "client-lua",
    category: "查询与工具",
    description: "向客户端聊天日志输出一条文字。",
    syntax: ["addCBLog(CBLOG_SYS, 文字)"],
    examples: [{ label: "输出提示", code: '$addCBLog(CBLOG_SYS, "测试提示")' }],
    source: "client/src/processrun.cpp · registerLuaExport()",
    keywords: ["日志", "输出"],
  },
  {
    id: "local-list-map",
    name: "listMap",
    entry: "server-local",
    category: "查询与工具",
    description: "在当前命令窗口列出服务器已加载地图 ID 和名称。",
    syntax: ["listMap()"],
    examples: [{ label: "列出地图", code: "listMap()" }],
    source: "server/src/server.lua · listMap()",
    keywords: ["地图", "列表"],
  },
  {
    id: "local-map-ids",
    name: "getMapIDList",
    entry: "server-local",
    category: "查询与工具",
    description: "返回服务器当前地图列表中的地图 ID。",
    syntax: ["getMapIDList()"],
    examples: [{ label: "获取已加载地图 ID", code: "getMapIDList()" }],
    notes: ["不是完整静态地图记录清单。"],
    source: "server/src/server.cpp · Server::regLuaExport()",
    keywords: ["地图", "查询"],
  },
  {
    id: "local-load-map",
    name: "loadMap",
    entry: "server-local",
    category: "宠物与世界",
    description: "加载指定基础地图。",
    syntax: ["loadMap(地图名)", "loadMap(mapID)"],
    examples: [{ label: "加载比奇基础地图", code: 'loadMap("比奇县_0")' }],
    source: "server/src/server.cpp · Server::regLuaExport()",
    keywords: ["比奇", "加载地图"],
  },
  {
    id: "local-count-monster",
    name: "countMonster",
    entry: "server-local",
    category: "宠物与世界",
    description: "查询指定基础地图上的指定怪物数量。",
    syntax: ["countMonster(monsterID, mapID)"],
    parameters: [
      { name: "monsterID", type: "整数", description: "怪物记录 ID。" },
      { name: "mapID", type: "整数", description: "基础地图 ID。" },
    ],
    examples: [
      {
        label: "查询比奇地图中的怪物数",
        code: 'countMonster(getMonsterID("稻草人"), getMapID("比奇县_0"))',
      },
    ],
    notes: ["查询失败时返回 -1。"],
    source: "server/src/server.cpp · Server::regLuaExport()",
    keywords: ["怪物", "比奇", "数量"],
  },
  {
    id: "local-add-monster",
    name: "addMonster",
    entry: "server-local",
    category: "宠物与世界",
    description: "在指定基础地图创建一只怪物。",
    syntax: [
      "addMonster(monsterID, mapID)",
      "addMonster(monsterID, mapID, x, y)",
      "addMonster(monsterID, mapID, x, y, strictLoc)",
    ],
    examples: [
      {
        label: "在比奇地图坐标生成怪物",
        code: 'addMonster(getMonsterID("稻草人"), getMapID("比奇县_0"), 414, 349, true)',
      },
    ],
    notes: ["立即创建实体，不会修改地图刷怪配置。最后一项是 strictLoc，不是旧注释写的 Random。"],
    source: "server/src/server.cpp · Server::regLuaExport()/Server::addMonster()",
    keywords: ["刷怪", "比奇", "生成怪物"],
  },
  {
    id: "local-log",
    name: "addCWLogString",
    entry: "server-local",
    category: "查询与工具",
    description: "向当前服务端命令窗口写一行日志。",
    syntax: ["addCWLogString(级别, 提示, 文字)"],
    examples: [{ label: "在命令窗口输出", code: 'addCWLogString(0, "> ", "操作完成")' }],
    source: "server/src/server.cpp · Server::regLuaExport()",
    keywords: ["输出", "日志"],
  },
  {
    id: "local-history",
    name: "history",
    entry: "server-local",
    category: "查询与工具",
    description: "显示当前命令窗口的输入历史。",
    syntax: ["history()"],
    examples: [{ label: "查看历史", code: "history()" }],
    source: "server/src/server.cpp · Server::regLuaExport()",
    keywords: ["历史", "命令"],
  },
  {
    id: "local-quit",
    name: "quit",
    entry: "server-local",
    category: "查询与工具",
    description: "关闭当前 Lua 命令窗口。",
    syntax: ["quit()"],
    examples: [{ label: "关闭命令窗口", code: "quit()" }],
    notes: ["不会关闭游戏服务器。"],
    source: "server/src/server.cpp · Server::regLuaExport()",
    keywords: ["关闭", "窗口"],
  },
  {
    id: "async-grant-gold",
    name: "给指定角色增加金币余额",
    entry: "server-async",
    category: "金币物品",
    description: "通过目标在线角色的 Player Lua addItem() 增加金币字段，不占背包格。",
    syntax: ['uidRemoteCall(playerUID, [[ addItem(getItemID("金币（小）"), count) ]])'],
    parameters: [
      {
        name: "playerUID",
        type: "在线玩家 UID",
        description: "不是角色 DBID。可由 DBID 按示例公式生成。",
      },
      { name: "count", type: "正整数", description: "增加的金币数。" },
    ],
    examples: [
      {
        label: "给 DBID 为 123 的在线角色加 10000 金币",
        code: 'local playerUID = (UID_PLY << 59) | 123\nuidRemoteCall(playerUID, [[\n    addItem(getItemID("金币（小）"), 10000)\n]])',
        template: true,
      },
    ],
    notes: [
      "将 123 替换为自己的角色 DBID；uidf::getPlayerUID(dbid) 使用 UID_PLY 左移 59 位后与 DBID 按位或。目标角色必须在线。",
      "金币余额独立于背包物品。不要用 @makeItem 或客户端 makeItem 充值。",
    ],
    source:
      "common/src/uidf.cpp · uidf::getPlayerUID()；common/src/luamodule.cpp · UID_PLY；server/src/player.cpp · addItem()",
    keywords: ["自己", "加钱", "金钱", "金币", "余额", "钱包", "背包", "DBID", "UID"],
  },
  {
    id: "async-drop-gold",
    name: "设置怪物死亡掉落金币",
    entry: "server-async",
    category: "金币物品",
    description: "为一个活动中的怪物设置指定金币掉落，怪物死亡后才会落到地面。",
    syntax: ["setMonsterDropOnDie(monsterUID, itemCfgList, opts)"],
    parameters: [
      { name: "monsterUID", type: "活动怪物 UID", description: "必须指向仍存活的怪物实例。" },
      { name: "itemCfgList", type: "掉落数组", description: "每项包含 item 与 odds。" },
      {
        name: "opts.defaultDrop",
        type: "布尔值，可选",
        description: "设为 true 保留该怪物原默认掉落。",
      },
    ],
    examples: [
      {
        label: "怪物死亡时额外掉落 10000 金币",
        code: 'setMonsterDropOnDie(MONSTER_UID, {\n    { item = { itemID = "金币（小）", count = 10000 }, odds = 1 },\n}, { defaultDrop = true })',
        template: true,
      },
    ],
    notes: [
      "将 MONSTER_UID 替换成当前活动怪物 UID；目标怪物还须允许正常死亡掉落。",
      "这不会立即在任意坐标生成金币，也不会改写地图或怪物配置。odds = 1 表示必定进入掉落结果。",
    ],
    source:
      "server/src/serverluacoroutinerunner.lua · setMonsterDropOnDie()；server/src/monster.cpp · Monster::onDie()",
    keywords: ["掉钱", "地面", "怪物", "金币", "掉落"],
  },
];

export function filterGMCommands(
  commands: GMCommand[],
  entry: GMEntry | "all",
  category: string,
  query: string,
): GMCommand[] {
  const terms = query.trim().toLocaleLowerCase().split(/\s+/u).filter(Boolean);
  return commands.filter((command) => {
    if (entry !== "all" && command.entry !== entry) return false;
    if (category !== "all" && command.category !== category) return false;
    const haystack = [
      command.name,
      command.description,
      command.category,
      GM_ENTRIES[command.entry].label,
      ...(command.syntax ?? []),
      ...(command.parameters ?? []).flatMap((parameter) => [
        parameter.name,
        parameter.type,
        parameter.description,
      ]),
      ...command.examples.flatMap((example) => [example.label, example.code]),
      ...(command.notes ?? []),
      ...(command.keywords ?? []),
    ]
      .join(" ")
      .toLocaleLowerCase();
    return terms.every((term) => haystack.includes(term));
  });
}
