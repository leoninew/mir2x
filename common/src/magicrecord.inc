{},

{   .name = u8"物理攻击", // TODO ...
    .castRange
    {
        .type = CRT_DIR,
        .distance = 1,
    },
},

// list of shared magic effect gfx
// these are not real magic entries, used as magic gfx entry ref

{   .name = u8"魔法特效_火焰灰烬", // used by 地狱火, 火墙
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X0000992,
            .frameCount = 10,
            .gfxDirType = 5,
            .onGround = 1,
            .loop = 1,
        },
    },
},

{   .name = u8"魔法特效_冰刺", // used by 冰沙掌, 魄冰刺
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X010005A,
            .frameCount = 20,
            .gfxDirType = 2,
            .onGround = 1,
            .speed = 200,
        },
    },
},

{   .name = u8"魔法特效_护身符",
    .seffBase = 13,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0000820,
            .frameCount = 6,
            .gfxDirType = 8,
        },

        {
            .stage = u8"运行",
            .type  = u8"跟随",
            .gfxID = 0X00003D4,
            .frameCount = 3,
            .gfxDirType = 16, //special, has 16 directions
            .loop = 1,
            .speed = 200,
        },
    },
},

{   .name = u8"魔法特效_召唤骷髅",
    .seffBase = 17,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X00002E4,
            .frameCount = 10,
            .motion = MOTION_SPELL1,
            .speed = 80,
        },

        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X00002EE,
            .frameCount = 10,
            .speed = 200,
        },
    },
},

{   .name = u8"魔法特效_雷电术",
    .seffBase = 11,
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X00005AA,
            .frameCount = 5,
        },
    },
},

{   .name = u8"魔法特效_火球术",
    .seffBase = 1,
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"跟随",
            .gfxID = 0X00001A4,
            .frameCount = 5,
            .gfxDirType = 16, //special, has 16 directions
            .loop = 1,
            .speed = 200,
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X0000244,
            .frameCount = 10,
        },
    },
},

{   .name = u8"魔法特效_大火球",
    .seffBase = 5,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0000618,
            .frameCount = 9,
            .gfxDirType = 8,
        },

        {
            .stage = u8"运行",
            .type  = u8"跟随",
            .gfxID = 0X0000668,
            .frameCount = 6,
            .gfxDirType = 16, //special, has 16 directions
            .loop = 1,
            .speed = 200,
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X0000708,
            .frameCount = 10,
        },
    },
},

{   .name = u8"魔法特效_瞬息移动",
    .seffBase = 21,
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"固定",  // don't use 附着
            .gfxID = 0X000006E,
            .frameCount = 10,
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X0000938,
            .frameCount = 10,
        },
    },
},

{   .name = u8"魔法特效_召唤雷电",
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0200118,
            .frameCount = 8,
            .gfxDirType = 1,
            .motion = MOTION_SPELL1,
        },
    },
},

{   .name = u8"祖玛教主_魔法特效_施法",
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X03002D0,
            .frameCount = 8,
            .gfxDirType = 8,
        },
    },
},

// end of shared magic effect
// rest are real magic effects and player/monster can cast

{   .name = u8"雷电术",
    .elem = u8"雷",
    .req
    {
        .level = {16, 18, 20},
        .train = {40, 50, 60},
        .job = u8"法师",
        .prior = u8"霹雳掌",
    },
    .coolDown = 800,
    .mp = 7,
    .mpInc = 6,
    .power = {15, 19},
    .addPower = {7, 11},
    .seffBase = 11,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0000596,
            .frameCount = 12,
            .motion = MOTION_SPELL1,
        },

        {
            .ref
            {
                .name = u8"魔法特效_雷电术",
                .stage = u8"运行",
            },
        },
    },
    .description = u8"召唤一道雷电劈击目标。以单体雷系伤害为主，适合远程集中火力对付一个敌人。",
},

{   .name = u8"魔法盾",
    .elem = u8"风",
    .req
    {
        .level = {29, 31, 33},
        .train = {90, 100, 110},
        .job = u8"法师",
        .prior = u8"抗拒火环",
    },
    .coolDown = 100,
    .mp = 20,
    .mpInc = 30,
    .seffBase = 31,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X000033E,
            .frameCount = 9,
            .motion = MOTION_SPELL1,
        },

        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X0000347,
            .frameCount = 10,
        },

        {
            .stage = u8"持续",
            .type  = u8"附着",
            .gfxID = 0X0000352,
            .frameCount = 3,
            .loop = 1,
        },

        {
            .stage = u8"挨打",
            .type  = u8"附着",
            .gfxID = 0X0000355,
            .frameCount = 3,
        },
    },
    .description = u8"在身外形成护盾，减轻受到的攻击伤害。法师的重要护身法术，持续受击会消耗护盾，需要留意保护状态。",
},

{   .name = u8"召唤骷髅",
    .elem = u8"幻影",
    .req
    {
        .level = {17, 19, 21},
        .train = {40, 50, 60},
        .job = u8"道士",
    },
    .coolDown = 2000,
    .mp = 15,
    .mpInc = 10,
    .gfxList
    {
        {
            .ref
            {
                .name  = u8"魔法特效_召唤骷髅",
                .stage = u8"启动",
            },
        },

        {
            .ref
            {
                .name  = u8"魔法特效_召唤骷髅",
                .stage = u8"运行",
            },
        },
    },
    .description = u8"召唤骷髅随从，协助自己战斗。骷髅会跟随主人并迎击敌人，适合分担近身作战的压力。",
},

{   .name = u8"火球术",
    .elem = u8"火",
    .req
    {
        .level = {7, 9, 11},
        .train = {10, 20, 30},
        .job = u8"法师",
    },
    .coolDown = 500,
    .checkGround = 1,
    .mp = 3,
    .mpInc = 1,
    .power = {6, 10},
    .addPower = {0, 4},
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X000071C,
            .frameCount = 8,
            .gfxDirType = 8,
        },

        {
            .ref
            {
                .name = u8"魔法特效_火球术",
                .stage = u8"运行",
            },
        },

        {
            .ref
            {
                .name = u8"魔法特效_火球术",
                .stage = u8"裂解",
            },
        },
    },
    .description = u8"发射火球，远程攻击单个目标。这是法师的入门火系法术，可在近战范围之外逐个攻击敌人。",
},

{   .name = u8"大火球",
    .elem = u8"火",
    .req
    {
        .level = {15, 17, 19},
        .train = {40, 50, 60},
        .job = u8"法师",
        .prior = u8"火球术",
    },
    .coolDown = 700,
    .checkGround = 1,
    .mp = 6,
    .mpInc = 6,
    .power = {15, 19},
    .addPower = {7, 11},
    .gfxList
    {
        {
            .ref
            {
                .name = u8"魔法特效_大火球",
                .stage = u8"启动",
            },
        },

        {
            .ref
            {
                .name = u8"魔法特效_大火球",
                .stage = u8"运行",
            },
        },

        {
            .ref
            {
                .name = u8"魔法特效_大火球",
                .stage = u8"裂解",
            },
        },
    },
    .description = u8"凝聚更强的火球，远程重击单个目标。是火球术的进阶法术，适合用更集中的火力打击单个敌人。",
},

{   .name = u8"治愈术",
    .elem = u8"神圣",
    .req
    {
        .level = {7, 9, 11},
        .train = {10, 20, 30},
        .job = u8"道士",
    },
    .coolDown = 400,
    .mp = 7,
    .mpInc = 2,
    .power = {11, 15},
    .seffBase = 2,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0000258,
            .frameCount = 10,
            .motion = MOTION_SPELL1,
        },

        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X0000262,
            .frameCount = 10,
        },
    },
    .description = u8"恢复自己或同伴的生命值。既可用于战斗间隙的自身恢复，也可为受伤同伴提供支援。",
},

{   .name = u8"基本剑术",
    .req
    {
        .level = {7, 9, 11},
        .train = {10, 20, 30},
        .job = u8"战士",
    },
    .description = u8"提高近战攻击的命中率。战士的入门剑法，学会后随普通攻击被动生效，无需主动施放。",
},

{   .name = u8"精神力战法",
    .req
    {
        .level = {8, 10, 12},
        .train = {10, 20, 30},
        .job = u8"道士",
    },
    .description = u8"以精神力强化剑术，提高近战攻击的命中率。属于被动生效的基础武功，无需主动施放，适合道士早期近身作战。",
},

{   .name = u8"施毒术",
    .elem = u8"暗黑",
    .req
    {
        .level = {12, 14, 16},
        .train = {20, 30, 40},
        .job = u8"道士",
    },
    .coolDown = 600,
    .mp = 10,
    .mpInc = 5,
    .seffBase = 6,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X000003C,
            .frameCount = 10,
            .motion = MOTION_SPELL1,
        },

        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X0000046,
            .frameCount = 10,
        },
    },
    .description = u8"使目标中毒，持续损失生命或降低防御。毒粉种类决定毒性，适合在持续交战前削弱目标并维持压制。",
},

{   .name = u8"攻杀剑术",
    .req
    {
        .level = {14, 16, 18},
        .train = {30, 40, 50},
        .job = u8"战士",
        .prior = u8"基本剑术",
    },
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X0000546,
            .frameCount = 6,
            .gfxDirType = 8,
        },
    },
    .description = u8"提高近战命中率，并有机会打出更强的一击。在普通攻击中发挥作用，是强化单体近战能力的进阶剑法。",
},

{   .name = u8"抗拒火环",
    .elem = u8"风",
    .req
    {
        .level = {12, 14, 16},
        .train = {20, 30, 40},
        .job = u8"法师",
        .prior = u8"风掌",
    },
    .coolDown = 600,
    .mp = 8,
    .mpInc = 1,
    .power = {0, 2},
    .seffBase = 8,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X000005A,
            .frameCount = 8,
            .motion = MOTION_SPELL1,
        },
    },
    .description = u8"释放环身火焰，推开靠近自己的敌人。用于受近身威胁时争取施法空间，能否推开目标受双方实力影响。",
},

{   .name = u8"疾光电影",
    .elem = u8"雷",
    .req
    {
        .level = {21, 23, 25},
        .train = {50, 60, 70},
        .job = u8"法师",
        .prior = u8"雷电术",
    },
    .coolDown = 800,
    .mp = 12,
    .mpInc = 15,
    .power = {14, 14},
    .addPower = {12, 16},
    .seffBase = 10,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X00007B2,
            .frameCount = 10,
            .gfxDirType = 8,
        },

        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X010049C,
            .frameCount = 6,
            .gfxDirType = 8,
        },
    },
    .description = u8"释放贯穿前方的雷电光束，攻击直线上的多个敌人。光束可穿过前方目标，适合沿通道或怪群排列方向集中扫射。",
},

{   .name = u8"刺杀剑术",
    .req
    {
        .level = {19, 21, 23},
        .train = {40, 50, 60},
        .job = u8"战士",
        .prior = u8"攻杀剑术",
    },
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X0000140,
            .frameCount = 6,
            .gfxDirType = 8,
        },
    },
    .description = u8"向前刺出剑气，攻击更远处的敌人。剑气沿面前方向延伸，适合保持间距打击直线上的目标。",
},

{   .name = u8"灵魂火符",
    .elem = u8"暗黑",
    .req
    {
        .level = {13, 15, 17},
        .train = {20, 30, 40},
        .job = u8"道士",
        .prior = u8"隐身术",
    },
    .coolDown = 500,
    .checkGround = 1,
    .mp = 6,
    .mpInc = 3,
    .power = {6, 10},
    .addPower = {3, 3},
    .seffBase = 13,
    .gfxList
    {
        {
            .ref
            {
                .name  = u8"魔法特效_护身符",
                .stage = u8"启动",
            },
        },

        {
            .ref
            {
                .name  = u8"魔法特效_护身符",
                .stage = u8"运行",
            },
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X0000474,
            .frameCount = 10,
        },
    },
    .description = u8"驱使护身符，远程攻击目标。施放需配合护身符，可在保持距离的同时配合毒术和随从作战。",
},

{   .name = u8"幽灵盾",
    .elem = u8"暗黑",
    .req
    {
        .level = {21, 23, 25},
        .train = {50, 60, 70},
        .job = u8"道士",
        .prior = u8"困魔咒",
    },
    .coolDown = 700,
    .mp = 10,
    .mpInc = 5,
    .seffBase = 14,
    .gfxList
    {
        {
            .ref
            {
                .name  = u8"魔法特效_护身符",
                .stage = u8"启动",
            },
        },

        {
            .ref
            {
                .name  = u8"魔法特效_护身符",
                .stage = u8"运行",
            },
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X00000C8,
            .frameCount = 10,
        },
    },
    .description = u8"暂时提高自己或同伴的魔法防御。适合应对擅长法术攻击的敌人，也可提前为队友施加防护。",
},

{   .name = u8"神圣战甲术",
    .elem = u8"暗黑",
    .req
    {
        .level = {25, 27, 29},
        .train = {70, 80, 90},
        .job = u8"道士",
        .prior = u8"幽灵盾",
    },
    .coolDown = 700,
    .mp = 10,
    .mpInc = 5,
    .seffBase = 15,
    .gfxList
    {
        {
            .ref
            {
                .name  = u8"魔法特效_护身符",
                .stage = u8"启动",
            },
        },

        {
            .ref
            {
                .name  = u8"魔法特效_护身符",
                .stage = u8"运行",
            },
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X00000AA,
            .frameCount = 10,
        },
    },
    .description = u8"暂时提高自己或同伴的物理防御。适合迎战近身攻击的怪物，也可为承受围攻的队友提前加持。",
},

{   .name = u8"困魔咒",
    .elem = u8"暗黑",
    .req
    {
        .level = {27, 29, 31},
        .train = {80, 90, 100},
        .job = u8"道士",
        .prior = u8"施毒术",
    },
    .coolDown = 1000,
    .mp = 15,
    .mpInc = 10,
    .seffBase = 16,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0000276,
            .frameCount = 10,
            .motion = MOTION_SPELL1,
        },

        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X0000280,
            .frameCount = 10,
        },
    },
    .description = u8"用咒阵暂时困住怪物，限制其活动范围。适合分隔怪群，攻击被困怪物或有人进入咒阵时会解除限制。",
},

{   .name = u8"隐身术",
    .elem = u8"暗黑",
    .req
    {
        .level = {20, 22, 24},
        .train = {50, 60, 70},
        .job = u8"道士",
        .prior = u8"施毒术",
    },
    .coolDown = 700,
    .mp = 5,
    .mpInc = 5,
    .seffBase = 18,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X000032A,
            .frameCount = 10,
            .motion = MOTION_SPELL1,
        },
    },
    .description = u8"隐藏自身踪迹，使多数怪物难以察觉。适合暂避围攻、争取恢复时间，部分感知敏锐的怪物仍能发现你。",
},

{   .name = u8"集体隐身术",
    .elem = u8"暗黑",
    .req
    {
        .level = {23, 25, 27},
        .train = {60, 70, 80},
        .job = u8"道士",
        .prior = u8"隐身术",
    },
    .coolDown = 700,
    .mp = 10,
    .mpInc = 5,
    .gfxList
    {
        {
            .ref
            {
                .name  = u8"魔法特效_护身符",
                .stage = u8"启动",
            },
        },

        {
            .ref
            {
                .name  = u8"魔法特效_护身符",
                .stage = u8"运行",
            },
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X0000334,
            .frameCount = 10,
        },
    },
    .description = u8"使指定区域内的同伴隐身，躲避多数怪物的察觉。适合队伍暂避怪群或原地整顿，为多名同伴争取安全空间。",
},

{   .name = u8"诱惑之光",
    .elem = u8"幻影",
    .req
    {
        .level = {13, 15, 17},
        .train = {20, 30, 40},
        .job = u8"法师",
    },
    .coolDown = 600,
    .mp = 3,
    .mpInc = 3,
    .seffBase = 20,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0000000,
            .frameCount = 10,
            .motion = MOTION_SPELL1,
        },

        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X000000A,
            .frameCount = 20,
        },
    },
    .description = u8"扰乱怪物的意识，有机会使其暂时成为自己的随从。控制并非必定成功，收服的怪物也不会永久追随。",
},

{   .name = u8"瞬息移动",
    .elem = u8"幻影",
    .req
    {
        .level = {14, 16, 18},
        .train = {30, 40, 50},
        .job = u8"法师",
    },
    .coolDown = 700,
    .mp = 10,
    .mpInc = 10,
    .gfxList
    {
        {
            .ref
            {
                .name = u8"魔法特效_瞬息移动",
                .stage = u8"运行",
            },
        },

        {
            .ref
            {
                .name = u8"魔法特效_瞬息移动",
                .stage = u8"裂解",
            },
        },
    },
    .description = u8"将自己随机传送到当前地图的其他位置。落点无法精确选择，适合摆脱追击或迅速离开危险区域。",
},

{   .name = u8"火墙",
    .elem = u8"火",
    .req
    {
        .level = {24, 26, 28},
        .train = {60, 70, 80},
        .job = u8"法师",
        .prior = u8"地狱火",
    },
    .coolDown = 1000,
    .mp = 22,
    .mpInc = 30,
    .power = {2, 9},
    .addPower = {1, 6},
    .seffBase = 22,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X000038E,
            .frameCount = 10,
            .motion = MOTION_SPELL1,
        },

        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X0000398,
            .frameCount = 5,
            .onGround = 1,
            .loop = 1,
            .speed = 50,
        },
    },
    .description = u8"在地面燃起持续火焰，灼烧进入其中的敌人。适合布置在怪物聚集或经过的位置，让敌人在移动中持续受伤。",
},

{   .name = u8"地狱火",
    .elem = u8"火",
    .req
    {
        .level = {20, 22, 24},
        .train = {50, 60, 70},
        .job = u8"法师",
        .prior = u8"大火球",
    },
    .coolDown = 800,
    .mp = 11,
    .mpInc = 15,
    .power = {14, 18},
    .addPower = {12, 16},
    .seffBase = 9,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X000071C,
            .frameCount = 8,
            .gfxDirType = 8,
        },

        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X000076C,
            .frameCount = 30,
            .gfxDirType = 2,
            .onGround = 1,
            .speed = 200,
        },
    },
    .description = u8"向前喷出一道火焰，攻击直线上的多个敌人。适合对付排成一列的怪物，调整站位可让火焰同时命中更多目标。",
},

{   .name = u8"爆裂火焰",
    .elem = u8"火",
    .req
    {
        .level = {32, 34, 36},
        .train = {100, 110, 120},
        .job = u8"法师",
        .prior = u8"地狱火",
    },
    .coolDown = 900,
    .mp = 15,
    .mpInc = 20,
    .power = {14, 18},
    .addPower = {14, 18},
    .seffBase = 23,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X00003AC,
            .frameCount = 10,
            .gfxDirType = 1,
            .motion = MOTION_SPELL1,
        },

        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X00003B6,
            .frameCount = 10,
            .gfxDirType = 1,
        },
    },
    .description = u8"在目标位置引爆火焰，攻击附近的多个敌人。以选定位置为中心造成范围伤害，适合清理紧密聚集的怪群。",
},

{   .name = u8"地狱雷光",
    .elem = u8"雷",
    .req
    {
        .level = {33, 35, 37},
        .train = {100, 110, 120},
        .job = u8"法师",
        .prior = u8"疾光电影",
    },
    .coolDown = 900,
    .mp = 17,
    .mpInc = 20,
    .power = {14, 18},
    .addPower = {14, 18},
    .seffBase = 24,
    .gfxList
    {
        {
            .ref
            {
                .name = u8"魔法特效_召唤雷电",
                .stage = u8"启动",
            },
        },

        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X01003D4,
            .frameCount = 8,
            .gfxDirType = 1,
        },
    },
    .description = u8"释放雷光风暴，攻击围在自己身边的敌人。以自身周围为打击区域，适合应对贴身怪群，施放时需留意自身安全。",
},

{   .name = u8"半月弯刀",
    .req
    {
        .level = {24, 26, 28},
        .train = {60, 70, 80},
        .job = u8"战士",
        .prior = u8"刺杀剑术",
    },
    .mpInc = 3,
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X00000E6,
            .frameCount = 6,
            .gfxDirType = 8,
        },
    },
    .description = u8"挥出半月形剑气，同时攻击身前的多个敌人。适合清理聚集在近身范围的怪物，被围攻时也能同时压制多个目标。",
},

{   .name = u8"烈火剑法",
    .req
    {
        .level = {32, 34, 36},
        .train = {100, 110, 120},
        .job = u8"战士",
        .prior = u8"半月弯刀",
    },
    .mp = 2,
    .mpInc = 7,
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X00005BE,
            .frameCount = 6,
            .gfxDirType = 8,
        },
    },
    .description = u8"将烈火凝聚在武器上，打出强力一击。将伤害集中在一次近战出手上，适合抓住接近目标的时机爆发。",
},

{   .name = u8"野蛮冲撞",
    .req
    {
        .level = {27, 29, 31},
        .train = {70, 80, 90},
        .job = u8"战士",
        .prior = u8"基本剑术",
    },
    .mp = 20,
    .description = u8"向前冲撞并推开敌人，突破包围。兼具短距离突进和控制作用，可用来脱身或打乱敌人的站位。",
},

{   .name = u8"群体治愈术",
    .elem = u8"神圣",
    .req
    {
        .level = {31, 33, 35},
        .train = {100, 110, 120},
        .job = u8"道士",
        .prior = u8"治愈术",
    },
    .coolDown = 400,
    .mp = 10,
    .mpInc = 20,
    .power = {8, 12},
    .addPower = {4, 4},
    .seffBase = 29,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0000294,
            .frameCount = 10,
            .motion = MOTION_SPELL1,
        },

        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X000029E,
            .frameCount = 10,
        },
    },
    .description = u8"同时恢复指定区域内多名同伴的生命值。适合多人受伤时集中支援，队友站位靠近可让一次治疗惠及更多人。",
},

{   .name = u8"召唤神兽",
    .elem = u8"幻影",
    .req
    {
        .level = {30, 32, 34},
        .train = {90, 100, 110},
        .job = u8"道士",
        .prior = u8"召唤骷髅",
    },
    .coolDown = 2000,
    .mp = 15,
    .mpInc = 15,
    .seffBase = 30,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0000A1E,
            .frameCount = 19,
            .motion = MOTION_SPELL1,
            .speed = 180,
        },
    },
    .description = u8"召唤会喷火的神兽，协助自己战斗。平时跟随主人，遇敌后以火焰迎击，是道士的重要战斗伙伴。",
},

{   .name = u8"圣言术",
    .elem = u8"幻影",
    .req
    {
        .level = {26, 28, 30},
        .train = {70, 80, 90},
        .job = u8"法师",
        .prior = u8"诱惑之光",
    },
    .coolDown = 1000,
    .mp = 30,
    .mpInc = 30,
    .seffBase = 32,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0000082,
            .frameCount = 10,
            .motion = MOTION_SPELL1,
        },

        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X000008C,
            .frameCount = 20,
        },
    },
    .description = u8"有机会直接消灭不死系怪物。成功率与双方等级及修炼程度有关，对有生命的怪物无效。",
},

{   .name = u8"冰咆哮",
    .elem = u8"冰",
    .req
    {
        .level = {34, 36, 38},
        .train = {100, 110, 120},
        .job = u8"法师",
        .prior = u8"冰沙掌",
    },
    .coolDown = 900,
    .mp = 19,
    .mpInc = 20,
    .power = {12, 16},
    .addPower = {14, 18},
    .seffBase = 33,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0000302,
            .frameCount = 10,
            .gfxDirType = 1,
            .motion = MOTION_SPELL1,
        },

        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X000030C,
            .frameCount = 10,
            .gfxDirType = 1,
        },
    },
    .description = u8"在目标区域卷起冰雪风暴，攻击其中的多个敌人。利用集中范围伤害覆盖怪群，适合连续打击聚集在同一区域的目标。",
},

{   .name = u8"莲月剑法",
    .req
    {
        .level = {38, 40, 42},
        .train = {100, 110, 120},
        .job = u8"战士",
        .prior = u8"烈火剑法",
    },
    .mp = 3,
    .mpInc = 9,
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X01006F4,
            .frameCount = 10,
            .gfxDirType = 8,
        },
    },
    .description = u8"以迅猛的剑光连续打击敌人，可衔接其他剑法。以紧凑连招压制近身目标，适合与其他强力剑法配合使用。",
},

{   .name = u8"翔空剑法",
    .req
    {
        .level = {35, 37, 39},
        .train = {100, 110, 120},
        .job = u8"战士",
        .prior = u8"半月弯刀",
    },
    .mp = 2,
    .mpInc = 8,
    .gfxList
    {
        {
            // TODO for 宝湾传奇 the 翔空剑法 is just an enhanced version of 半月弯刀, so the gfx index is 2185, and using motion MOTION_WHEELWIND, it's a jump
            // I remember when I play the private server, 翔空剑法 is ``在从半空落下攻击敌人的技能", which means a jump, the corresponding gfx index is 2270, which is ``random swing".
            // and in the motion set there is a motion called MOTION_RANDSWING, it's purely for gfx index 2270, but it's NOT a jump
            // here I use MOTION_RANDSWING and the gfx 2270 to present 翔空剑法

            // below are all confusing gfx:
            //
            //  230 : 半月弯刀
            //  320 : 刺杀剑术
            // 1350 : 攻杀剑术
            // 2185 : 升级版半月弯刀, used by 宝湾传奇 as 翔空剑法
            // 2270 : random swing, take it as 翔空剑法

            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X00008DE,
            .frameCount = 10,
            .gfxDirType = 8,
        },
    },
    .description = u8"跃起后挥剑重击敌人，可衔接其他剑法。借腾跃之势集中力量，适合在贴近目标后衔接爆发攻击。",
},

{   .name = u8"空拳刀法",
    .req
    {
        .level = {28, 30, 32},
        .train = {90, 100, 110},
        .job = u8"道士",
        .prior = u8"精神力战法",
    },
    .coolDown = 800,
    .mp = 20,
    .mpInc = 10,
    .description = u8"以拳脚和真气推开近身敌人，摆脱包围。适合被近身围堵时开路，为治疗或重新调整站位争取时间。",
},

{   .name = u8"月魂断玉",
    .elem = u8"神圣",
    .req
    {
        .level = {14, 16, 18},
        .train = {30, 40, 50},
        .job = u8"道士",
        .prior = u8"治愈术",
    },
    .coolDown = 500,
    .mp = 6,
    .mpInc = 3,
    .power = {6, 10},
    .addPower = {3, 3},
    .seffBase = 37,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0000CB2,
            .frameCount = 6,
            .gfxDirType = 8,
        },

        {
            .stage = u8"运行",
            .type  = u8"跟随",
            .gfxID = 0X0000D02,
            .frameCount = 6,
            .gfxDirType = 1,
            .loop = 1,
            .speed = 200,
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X0000D0C,
            .frameCount = 10,
            .gfxDirType = 1,
        },
    },
    .description = u8"凝聚神圣之力远程攻击目标，无需消耗护身符。对不死系怪物尤为有效，可作为道士的基础神圣系攻击手段。",
},

{   .name = u8"月魂灵波",
    .elem = u8"神圣",
    .req
    {
        .level = {24, 26, 28},
        .train = {60, 70, 80},
        .job = u8"道士",
        .prior = u8"月魂断玉",
    },
    .mp = 8,
    .mpInc = 4,
    .power = {8, 12},
    .addPower = {7, 7},
    .seffBase = 38,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0000D20,
            .frameCount = 6,
            .gfxDirType = 8,
        },

        {
            .stage = u8"运行",
            .type  = u8"跟随",
            .gfxID = 0X0000D70,
            .frameCount = 6,
            .gfxDirType = 1,
            .loop = 1,
            .speed = 200,
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X0000D7A,
            .frameCount = 10,
            .gfxDirType = 1,
        },
    },
    .description = u8"释放更强的神圣灵波攻击目标，无需消耗护身符。是月魂断玉的进阶法术，适合远程集中打击惧怕神圣力量的敌人。",
},

{   .name = u8"冰月神掌",
    .elem = u8"冰",
    .req
    {
        .level = {9, 11, 13},
        .train = {10, 20, 30},
        .job = u8"法师",
    },
    .coolDown = 500,
    .checkGround = 1,
    .mp = 4,
    .mpInc = 1,
    .power = {4, 8},
    .addPower = {0, 4},
    .seffBase = 39,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0000A3C,
            .frameCount = 6,
            .gfxDirType = 8,
        },

        {
            .stage = u8"运行",
            .type  = u8"跟随",
            .gfxID = 0X0000A8C,
            .frameCount = 3,
            .gfxDirType = 16, //special, has 16 directions
            .loop = 1,
            .speed = 200,
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X0000B2C,
            .frameCount = 10,
        },
    },
    .description = u8"发射冰寒掌力攻击目标，并有机会使其减速。以单体牵制为主，可在削弱追击速度的同时拉开距离。",
},

{   .name = u8"冰月震天",
    .elem = u8"冰",
    .req
    {
        .level = {17, 19, 21},
        .train = {40, 50, 60},
        .job = u8"法师",
        .prior = u8"冰月神掌",
    },
    .coolDown = 700,
    .checkGround = 1,
    .mp = 7,
    .mpInc = 6,
    .power = {13, 17},
    .addPower = {7, 11},
    .seffBase = 40,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0000B40,
            .frameCount = 6,
            .gfxDirType = 8,
        },

        {
            .stage = u8"运行",
            .type  = u8"跟随",
            .gfxID = 0X0000B90,
            .frameCount = 6,
            .gfxDirType = 1,
            .loop = 1,
            .speed = 200,
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X0000B9A,
            .frameCount = 10,
        },
    },
    .description = u8"以更强的冰球攻击目标，并有机会使其减速。是冰月神掌的进阶法术，在增强单体伤害的同时保留冰系牵制能力。",
},

{   .name = u8"霹雳掌",
    .elem = u8"雷",
    .req
    {
        .level = {8, 10, 12},
        .train = {10, 20, 30},
        .job = u8"法师",
    },
    .coolDown = 500,
    .mp = 4,
    .mpInc = 1,
    .power = {6, 10},
    .addPower = {0, 4},
    .seffBase = 41,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0000BAE,
            .frameCount = 6,
            .gfxDirType = 8,
        },

        {
            .stage = u8"运行",
            .type  = u8"跟随",
            .gfxID = 0X0000BFE,
            .frameCount = 6,
            .gfxDirType = 16, //special, has 16 directions
            .loop = 1,
            .speed = 200,
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X0000C9E,
            .frameCount = 10,
            .gfxDirType = 1,
        },
    },
    .description = u8"从掌心释放电击，远程攻击目标。属于基础雷系法术，适合在保持距离时对付单个敌人。",
},

{   .name = u8"冰沙掌",
    .elem = u8"冰",
    .req
    {
        .level = {22, 24, 26},
        .train = {50, 60, 70},
        .job = u8"法师",
        .prior = u8"冰月震天",
    },
    .coolDown = 800,
    .checkGround = 1,
    .mp = 12,
    .mpInc = 15,
    .power = {12, 16},
    .addPower = {12, 16},
    .seffBase = 53,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0100000,
            .frameCount = 10,
            .gfxDirType = 8,
        },

        {
            .stage = u8"运行",  // gfx uses 冰刺
            .type  = u8"固定",
            .onGround = 1,
            .speed = 200,
        },
    },
    .description = u8"向前释放一线寒气，攻击并减缓敌人的行动。兼顾直线范围伤害与冰系牵制，适合拖慢成列敌人的追击。",
},

{   .name = u8"风掌",
    .elem = u8"风",
    .req
    {
        .level = {10, 12, 14},
        .train = {10, 20, 30},
        .job = u8"法师",
    },
    .coolDown = 500,
    .mp = 3,
    .mpInc = 1,
    .power = {5, 9},
    .addPower = {0, 4},
    .seffBase = 67,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X010015E,
            .modColor = colorf::RGBA(255, 255, 255, 120),
            .frameCount = 7,
            .gfxDirType = 8,
        },

        {
            .stage = u8"运行",
            .type  = u8"跟随",
            .gfxID = 0X01001AE,
            .modColor = colorf::RGBA(255, 255, 255, 120),
            .frameCount = 5,
            .gfxDirType = 16, //special, has 16 directions
            .loop = 1,
            .speed = 200,
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X010024E,
            .modColor = colorf::RGBA(255, 255, 255, 120),
            .frameCount = 10,
            .gfxDirType = 1,
        },
    },
    .description = u8"从掌心释放风力，远程攻击目标。属于基础风系法术，适合对付风元素防御较弱的敌人。",
},

{   .name = u8"龙卷风",
    .elem = u8"风",
    .req
    {
        .level = {35, 37, 39},
        .train = {100, 110, 120},
        .job = u8"法师",
        .prior = u8"风震天",
    },
    .coolDown = 900,
    .mp = 18,
    .mpInc = 20,
    .power = {13, 17},
    .addPower = {14, 18},
    .seffBase = 72,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0100406,
            .frameCount = 10,
            .gfxDirType = 1,
            .motion = MOTION_SPELL1,
        },

        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X0100410,
            .frameCount = 16,
            .gfxDirType = 1,
        },
    },
    .description = u8"在目标区域召唤旋风，持续攻击其中的敌人。适合压制聚集或停留在一处的怪群，让旋风的持续伤害充分发挥。",
},

{   .name = u8"风震天",
    .elem = u8"风",
    .req
    {
        .level = {23, 25, 27},
        .train = {60, 70, 80},
        .job = u8"法师",
        .prior = u8"击风",
    },
    .coolDown = 800,
    .mp = 13,
    .mpInc = 15,
    .power = {13, 17},
    .addPower = {12, 16},
    .seffBase = 73,
    .gfxList
    {
        // 击风 and 风震天 share 启动 and 裂解
        // 风震天 运行 is the last 5 frames of 击风 运行, moving straight from the caster
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X01007B2, // same with 击风
            .frameCount = 10,
            .gfxDirType = 1,
            .motion = MOTION_SPELL1,
        },

        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X01007C6,
            .frameCount = 5,
            .gfxDirType = 1,
        },

        {
            .stage = u8"裂解",
            .type  = u8"固定",
            .gfxID = 0X01007D0,
            .frameCount = 10,
            .gfxDirType = 1,
        },
    },
    .description = u8"向前释放强烈气流，攻击并击退直线上的敌人。适合清理前方通道，在造成范围伤害的同时争取安全距离。",
},

{   .name = u8"击风",
    .elem = u8"风",
    .req
    {
        .level = {18, 20, 22},
        .train = {40, 50, 60},
        .job = u8"法师",
        .prior = u8"风掌",
    },
    .coolDown = 800,
    .mp = 6,
    .mpInc = 6,
    .power = {14, 18},
    .addPower = {7, 11},
    .seffBase = 74,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X01007B2,
            .frameCount = 10,
            .gfxDirType = 1,
            .motion = MOTION_SPELL1,
        },

        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X01007BC,
            .frameCount = 15,
            .gfxDirType = 1,
        },

        {
            .stage = u8"裂解",
            .type  = u8"固定",
            .gfxID = 0X01007D0,
            .frameCount = 10,
            .gfxDirType = 1,
        },
    },
    .description = u8"以强风攻击目标，并有机会将其击退。是风掌的进阶法术，适合利用击退效果与近战敌人保持距离。",
},

{   .name = u8"回生术",
    .elem = u8"神圣",
    .req
    {
        .level = {35, 37, 39},
        .train = {50, 60, 70},
        .job = u8"道士",
        .prior = u8"群体治愈术",
    },
    .coolDown = 5000,
    .mp = 100,
    .mpInc = 100,
    .seffBase = 77,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0100136,
            .frameCount = 10,
            .motion = MOTION_SPELL1,
        },

        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X0100140,
            .frameCount = 10,
        },
    },
    .description = u8"使倒下的同伴复活。施放需要护身符，并需同伴允许接受复活，适合在队伍减员时提供救援。",
},

{   .name = u8"强魔震法",
    .elem = u8"暗黑",
    .req
    {
        .level = {29, 31, 33},
        .train = {90, 100, 110},
        .job = u8"道士",
        .prior = u8"神圣战甲术",
    },
    .coolDown = 700,
    .mp = 10,
    .mpInc = 5,
    .seffBase = 89,
    .gfxList
    {
        {
            .ref
            {
                .name  = u8"魔法特效_护身符",
                .stage = u8"启动",
            },
        },

        {
            .ref
            {
                .name  = u8"魔法特效_护身符",
                .stage = u8"运行",
            },
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X010074E,
            .frameCount = 10,
        },
    },
    .description = u8"暂时提高自己或同伴的元素魔法攻击力。增强火、冰、雷、风等元素法术的威力，尤其适合支援法师作战。",
},

{   .name = u8"猛虎强势",
    .elem = u8"暗黑",
    .req
    {
        .level = {34, 36, 38},
        .train = {100, 110, 120},
        .job = u8"道士",
        .prior = u8"强魔震法",
    },
    .coolDown = 700,
    .mp = 10,
    .mpInc = 5,
    .seffBase = 94,
    .gfxList
    {
        {
            .ref
            {
                .name  = u8"魔法特效_护身符",
                .stage = u8"启动",
            },
        },

        {
            .ref
            {
                .name  = u8"魔法特效_护身符",
                .stage = u8"运行",
            },
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X010008C,
            .frameCount = 10,
        },
    },
    .description = u8"暂时提高自己和附近同伴的物理攻击力。适合近战队伍交战前加持，帮助战士和其他物理攻击者提高输出。",
},

{   .name = u8"铁布衫",
    .req
    {
        .level = {44, 47, 50},
        .train = {1000, 2000, 3000},
        .job = u8"战士",
        .prior = u8"乾坤大挪移",
    },
    .coolDown = 1000,
    .mp = 40,
    .mpInc = 20,
    .power = {2, 4},
    .addPower = {3, 5},
    .seffBase = 102,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0200028,
            .modColor = colorf::RGBA(255, 255, 255, 120),
            .frameCount = 10,
            .gfxDirType = 1,
            .motion = MOTION_ATTACKMODE,
        },
    },
    .description = u8"暂时降低自身攻击力，提高物理和魔法防御。适合承受围攻或强敌重击，以部分输出能力换取更好的生存能力。",
},

{   .name = u8"十方斩",
    .req
    {
        .level = {40, 43, 46},
        .train = {500, 800, 1000},
        .job = u8"战士",
        .prior = u8"翔空剑法",
    },
    .mpInc = 7,
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X0200208,
            .frameCount = 6,
            .gfxDirType = 1,
        },
    },
    .description = u8"释放环身剑气，攻击周围的敌人。正面剑气的打击最强，适合在怪物密集时发挥近身范围伤害。",
},

{   .name = u8"异形换位",
    .elem = u8"幻影",
    .req
    {
        .level = {27, 29, 31},
        .train = {80, 90, 100},
        .job = u8"法师",
        .prior = u8"瞬息移动",
    },
    .coolDown = 2500,
    .mp = 20,
    .mpInc = 20,
    .seffBase = 104,
    .gfxList
    {
        {
            .ref
            {
                .name = u8"魔法特效_瞬息移动",
                .stage = u8"运行",
            },
        },

        {
            .ref
            {
                .name = u8"魔法特效_瞬息移动",
                .stage = u8"裂解",
            },
        },
    },
    .description = u8"将自己瞬间传送到视野内选定的位置。可主动选择落点，用于调整施法站位、越过危险或脱离包围。",
},

{   .name = u8"超强召唤骷髅",
    .elem = u8"幻影",
    .req
    {
        .level = {33, 35, 37},
        .train = {100, 110, 120},
        .job = u8"道士",
        .prior = u8"召唤神兽",
    },
    .coolDown = 2000,
    .mp = 20,
    .mpInc = 25,
    .seffBase = 105,
    .gfxList
    {
        {
            .ref
            {
                .name  = u8"魔法特效_召唤骷髅",
                .stage = u8"启动",
            },
        },

        {
            .ref
            {
                .name  = u8"魔法特效_召唤骷髅",
                .stage = u8"运行",
            },
        },
    },
    .description = u8"召唤更强的骷髅随从，可与神兽共同战斗。适合以多个召唤随从共同分担战斗压力，增强持续作战能力。",
},

{   .name = u8"破血狂杀",
    .req
    {
        .level = {48, 51, 54},
        .train = {1000, 2000, 3000},
        .job = u8"战士",
        .prior = u8"斗转星移",
    },
    .coolDown = 2000,
    .mp = 100,
    .mpInc = 100,
    .power = {2, 4},
    .addPower = {3, 5},
    .seffBase = 106,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X020003C,
            .modColor = colorf::RGBA(255, 255, 255, 100),
            .frameCount = 10,
            .gfxDirType = 1,
            .motion = MOTION_ATTACKMODE,
        },
    },
    .description = u8"暂时降低物理和魔法防御，大幅提高自身攻击力。适合追求短时间爆发，但防御降低后也更容易受到重创。",
},

{   .name = u8"乾坤大挪移",
    .req
    {
        .level = {42, 45, 48},
        .train = {500, 800, 1000},
        .job = u8"战士",
        .prior = u8"野蛮冲撞",
    },
    .coolDown = 200,
    .mp = 40,
    .mpInc = 10,
    .seffBase = 107,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0200000,
            .frameCount = 10,
            .gfxDirType = 1,
            .motion = MOTION_SPELL1,
        },

        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X020000A,
            .frameCount = 10,
            .gfxDirType = 1,
        },
    },
    .description = u8"与目标交换位置，改变双方的站位。可借此脱离怪群包围，或将敌人换入对己方有利的位置。",
},

{   .name = u8"斗转星移",
    .req
    {
        .level = {46, 49, 52},
        .train = {500, 800, 1000},
        .job = u8"战士",
        .prior = u8"铁布衫",
    },
    .coolDown = 200,
    .mp = 40,
    .mpInc = 20,
    .seffBase = 108,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0200244,
            .frameCount = 10,
            .gfxDirType = 8,
        },
    },
    .description = u8"将远处的目标拉到自己面前。适合把远程敌人拖入近战范围，再衔接剑法集中攻击。",
},

{   .name = u8"魄冰刺",
    .elem = u8"冰",
    .req
    {
        .level = {38, 41, 44},
        .train = {500, 800, 1000},
        .job = u8"法师",
        .prior = u8"冰咆哮",
    },
    .coolDown = 100,
    .mp = 20,
    .mpInc = 20,
    .power = {16, 20},
    .addPower = {16, 20},
    .seffBase = 110,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0100000,
            .frameCount = 10,
            .gfxDirType = 8,
        },
    },
    .description = u8"向前放出三道冰刺，攻击并减缓成片敌人的行动。覆盖比单线冰系法术更宽的前方区域，适合在开阔地形迎击怪群。",
},

{   .name = u8"怒神霹雳",
    .elem = u8"雷",
    .req
    {
        .level = {40, 42, 44},
        .train = {500, 800, 1000},
        .job = u8"法师",
        .prior = u8"地狱雷光",
    },
    .coolDown = 100,
    .mp = 40,
    .mpInc = 30,
    .power = {20, 40},
    .addPower = {20, 30},
    .seffBase = 111,
    .gfxList
    {
        {
            .ref
            {
                .name = u8"魔法特效_召唤雷电",
                .stage = u8"启动",
            },
        },

        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X02001D6,
            .frameCount = 10,
            .gfxDirType = 1,
        },
    },
    .description = u8"在目标位置引发雷电爆炸，越靠近中心伤害越高。适合打击密集怪群，施法时将主要目标置于爆炸中心可发挥更大威力。",
},

{   .name = u8"凝血离魂",
    .elem = u8"幻影",
    .req
    {
        .level = {46, 48, 50},
        .train = {1000, 2000, 3000},
        .job = u8"法师",
        .prior = u8"圣言术",
    },
    .coolDown = 100,
    .mp = 60,
    .mpInc = 10,
    .power = {6, 2},
    .addPower = {1, 1},
    .seffBase = 112,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0200050,
            .frameCount = 10,
            .motion = MOTION_SPELL1,
        },
    },
    .description = u8"暂时减少自身生命上限，提高魔法攻击力。以承受伤害的余地换取输出能力，适合能维持安全施法距离的战斗。",
},

{   .name = u8"焰天火雨",
    .elem = u8"火",
    .req
    {
        .level = {43, 45, 47},
        .train = {1000, 2000, 3000},
        .job = u8"法师",
        .prior = u8"大火球",
    },
    .coolDown = 100,
    .mp = 32,
    .mpInc = 20,
    .power = {32, 40},
    .addPower = {14, 22},
    .seffBase = 113,
    .gfxList
    {
        {
            .ref
            {
                .name = u8"魔法特效_大火球",
                .stage = u8"启动",
            },
        },

        {
            .ref
            {
                .name = u8"魔法特效_大火球",
                .stage = u8"运行",
            },
        },

        {
            .ref
            {
                .name = u8"魔法特效_大火球",
                .stage = u8"裂解",
            },
        },
    },
    .description = u8"向前方扇形区域降下火雨，攻击成片敌人。适合攻击大批聚集在前方的怪物，调整朝向可覆盖更多目标。",
},

{   .name = u8"云寂术",
    .elem = u8"神圣",
    .req
    {
        .level = {38, 41, 44},
        .train = {500, 800, 1000},
        .job = u8"道士",
        .prior = u8"月魂灵波",
    },
    .coolDown = 100,
    .mp = 20,
    .mpInc = 10,
    .seffBase = 120,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X02000DC,
            .frameCount = 10,
            .motion = MOTION_SPELL1,
        },

        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X02000E6,
            .frameCount = 10,
        },
    },
    .description = u8"清除目标身上的增益和减益效果。可清除中毒、麻痹等状态，也会一并移除防御和攻击加持，施放前需判断目标状态。",
},

{   .name = u8"妙影无踪",
    .elem = u8"暗黑",
    .req
    {
        .level = {43, 45, 47},
        .train = {1000, 2000, 3000},
        .job = u8"道士",
        .prior = u8"集体隐身术",
    },
    .coolDown = 100,
    .mp = 120,
    .mpInc = 80,
    .seffBase = 121,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X02001AE,
            .frameCount = 7,
            .motion = MOTION_ATTACKMODE,
        },
    },
    .description = u8"使自己隐去身形，移动后解除隐蔽。可躲避怪物与其他玩家的察觉，适合藏身恢复或暂避追击。",
},

{   .name = u8"阴阳法环",
    .elem = u8"神圣",
    .req
    {
        .level = {46, 48, 50},
        .train = {1000, 2000, 3000},
        .job = u8"道士",
        .prior = u8"云寂术",
    },
    .coolDown = 200,
    .mp = 60,
    .mpInc = 50,
    .seffBase = 122,
    .gfxList
    {
        {
            .ref
            {
                .name = u8"魔法特效_召唤雷电",
                .stage = u8"启动",
                .modColor = colorf::RGBA(255, 255, 126, 80),
            },
        },

        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X0200122,
            .modColor = colorf::RGBA(255, 255, 255, 100),
            .frameCount = 9,
        },

        {
            .stage = u8"持续",
            .type  = u8"附着",
            .gfxID = 0X020012C,
            .modColor = colorf::RGBA(255, 255, 255, 100),
            .frameCount = 3,
            .loop = 1,
        },

        {
            .stage = u8"挨打",
            .type  = u8"附着",
            .gfxID = 0X020012F,
            .modColor = colorf::RGBA(255, 255, 255, 100),
            .frameCount = 3,
        },
    },
    .description = u8"抵挡一次致命攻击并恢复少量生命，随后法环消失。平时受到非致命伤害时不会触发，适合提前加持以防突然遭受重击。",
},

{   .name = u8"移花接玉",
    .elem = u8"幻影",
    .req
    {
        .level = {40, 42, 44},
        .train = {500, 800, 1000},
        .job = u8"道士",
        .prior = u8"超强召唤骷髅",
    },
    .coolDown = 200,
    .mp = 40,
    .mpInc = 30,
    .power = {4, 8},
    .addPower = {3, 4},
    .seffBase = 123,
    .gfxList
    {
        {
            .stage = u8"启动",
            .type  = u8"附着",
            .gfxID = 0X0200168,
            .frameCount = 10,
            .motion = MOTION_SPELL1,
        },

        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X0200172,
            .frameCount = 10,
        },
    },
    .description = u8"暂时降低自身物理攻击力，增强自己的召唤随从。只强化自己的召唤物，适合主要依靠随从输出的作战方式。",
},

{   .name = u8"神兽_喷火",
    .elem = u8"火",
    .castRange
    {
        .type = CRT_DIR,
        .distance = 2,
    },

    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X03003D4,
            .frameCount = 6,
            .gfxDirType = 8,
        },
    },
},

{   .name = u8"沙漠石人_石坑",
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X0F000C8,
            .frameCount = 1,
            .gfxDirType = 8,
            .onGround = 1,
            .loop = 1,
        },
    },
},

{   .name = u8"沙漠石人_死亡特效",
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X0300132,
            .frameCount = 8,
            .gfxDirType = 1,
        },
    },
},

{   .name = u8"雷电僵尸_雷电",
    .elem = u8"雷",

    .castRange
    {
        .type = CRT_DIR,
        .distance = 8,
    },

    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X0300258,
            .frameCount = 6,
            .gfxDirType = 8,
        },
    },
},

{   .name = u8"僧侣僵尸_地洞",
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X0F000F0,
            .frameCount = 1,
            .gfxDirType = 8,
            .onGround = 1,
            .loop = 1,
        },
    },
},

{   .name = u8"沙漠树魔_喷刺",
    .castRange
    {
        .type = CRT_LIMITED,
        .distance = 5,
    },

    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"跟随",
            .gfxID = 0X03003C0,
            .frameCount = 1,
            .gfxDirType = 1,
            .loop = 1,
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X01003A2,
            .frameCount = 6,
        },
    },
},

{   .name = u8"楔蛾_喷毒",
    .castRange
    {
        .type = CRT_DIR,
        .distance = 1,
    },

    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X0300370,
            .frameCount = 6,
            .gfxDirType = 8,
        },
    },
},

{   .name = u8"掷斧骷髅_掷斧",
    .castRange
    {
        .type = CRT_LIMITED,
        .distance = 5,
    },

    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"跟随",
            .gfxID = 0X0300320,
            .frameCount = 6,
            .gfxDirType = 8,
            .loop = 1,
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X01003A2,
            .frameCount = 6,
        },
    },
},

{   .name = u8"诺玛法老_火球术",
    .elem = u8"火",

    .castRange
    {
        .type = CRT_LIMITED,
        .distance = 8,
    },

    .gfxList
    {
        {
            .ref
            {
                .name = u8"魔法特效_火球术",
                .stage = u8"运行",
            },
        },

        {
            .ref
            {
                .name = u8"魔法特效_火球术",
                .stage = u8"裂解",
            },
        },
    },
},

{   .name = u8"诺玛大法老_雷电术",
    .elem = u8"雷",

    .castRange
    {
        .type = CRT_LIMITED,
        .distance = 10,
    },

    .gfxList
    {
        {
            .ref
            {
                .name = u8"魔法特效_雷电术",
                .stage = u8"运行",
            },
        },
    },
},

{   .name = u8"稻草人_死亡特效",
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着", // may also make it as 固定, magic of type 附着 only get draw when monster body is still visible
            .gfxID = 0X03002A8,
            .frameCount = 10,
        },
    },
},

{   .name = u8"轻甲守卫_死亡特效",
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着", // may also make it as 固定, magic of type 附着 only get draw when monster body is still visible
            .gfxID = 0X030003C,
            .frameCount = 18,
        },
    },
},

{   .name = u8"蚂蚁道士_治疗",
    .castRange
    {
        .type = CRT_LIMITED,
        .distance = 8,
    },

    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X0300064,
            .frameCount = 10,
        },
    },
},

{   .name = u8"红衣法师_魔法",
    .castRange
    {
        .type = CRT_LIMITED,
        .distance = 8,
    },

    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X0300118,
            .frameCount = 10,
        },
    },
},

{   .name = u8"红衣法师_死亡特效",
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X030003C,
            .frameCount = 18,
        },
    },
},

{   .name = u8"骷髅精灵_死亡特效",
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X0300780,
            .frameCount = 10,
        },
    },
},

{   .name = u8"洞蛆_喷毒",
    .castRange
    {
        .type = CRT_DIR,
        .distance = 1,
    },

    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X0300794,
            .frameCount = 5,
            .gfxDirType = 8,
        },
    },
},

{   .name = u8"火焰沃玛_喷火",
    .castRange
    {
        .type = CRT_DIR,
        .distance = 1,
    },

    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X0300208,
            .frameCount = 7,
            .gfxDirType = 8,
        },
    },
},

{   .name = u8"沃玛教主_电光",
    .elem = u8"雷",

    .castRange
    {
        .type = CRT_DIR,
        .distance = 1,
    },

    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X03001B8,
            .frameCount = 6,
            .gfxDirType = 8,
        },
    },
},

{   .name = u8"沃玛教主_雷电术",
    .elem = u8"雷",

    .castRange
    {
        .type = CRT_LIMITED,
        .distance = 10,
    },

    .gfxList
    {
        {
            .ref
            {
                .name = u8"魔法特效_雷电术",
                .stage = u8"运行",
            },
        },
    },
},

{   .name = u8"暗黑战士_喷刺",
    .castRange
    {
        .type = CRT_LIMITED,
        .distance = 8,
    },

    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"跟随",
            .gfxID = 0X03004D8,
            .frameCount = 1,
            .gfxDirType = 16,
            .loop = 1,
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X01003A2,
            .frameCount = 6,
        },
    },
},

{   .name = u8"粪虫_喷毒",
    .castRange
    {
        .type = CRT_DIR,
        .distance = 1,
    },

    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X03007EE,
            .frameCount = 5,
            .gfxDirType = 8,
        },
    },
},

{   .name = u8"祖玛弓箭手_射箭",
    .castRange
    {
        .type = CRT_LIMITED,
        .distance = 8,
    },

    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"跟随",
            .gfxID = 0X0300578,
            .frameCount = 1,
            .gfxDirType = 16,
            .loop = 1,
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X01003A2,
            .frameCount = 6,
        },
    },
},

{   .name = u8"祖玛教主_石像碎片",
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"固定",
            .gfxID = 0X0F000D2,
            .frameCount = 1,
            .gfxDirType = 1,
            .onGround = 1,
            .loop = 1,
        },
    },
},

{   .name = u8"祖玛教主_火墙",
    .elem = u8"火",

    .castRange
    {
        .type = CRT_LIMITED,
        .distance = 6,
    },

    .gfxList
    {
        {
            .ref
            {
                .name = u8"祖玛教主_魔法特效_施法",
                .stage = u8"启动",
            },
        },
    },
},

{   .name = u8"祖玛教主_地狱火",
    .elem = u8"火",

    .castRange
    {
        .type = CRT_DIR,
        .distance = 6,
    },

    .gfxList
    {
        {
            .ref
            {
                .name = u8"祖玛教主_魔法特效_施法",
                .stage = u8"启动",
            },
        },
    },
},

{   .name = u8"爆毒蚂蚁_喷毒",
    .castRange
    {
        .type = CRT_LIMITED,
        .distance = 5,
    },

    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"跟随",
            .gfxID = 0X0300050,
            .frameCount = 6,
            .gfxDirType = 1,
            .loop = 1,
        },
    },
},

{   .name = u8"沙漠风魔_扇风",
    .castRange
    {
        .type = CRT_LIMITED,
        .distance = 5,
    },

    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X0300622,
            .frameCount = 10,
            .gfxDirType = 1,
        },
    },
},

{   .name = u8"神舰守卫_死亡特效",
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着", // may also make it as 固定, magic of type 附着 only get draw when monster body is still visible
            .gfxID = 0X0300104,
            .frameCount = 6,
        },
    },
},

{   .name = u8"月魔蜘蛛_死亡特效",
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着", // may also make it as 固定, magic of type 附着 only get draw when monster body is still visible
            .gfxID = 0X03008CA,
            .frameCount = 20,
        },
    },
},

{   .name = u8"爆裂蜘蛛_死亡特效",
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着", // may also make it as 固定, magic of type 附着 only get draw when monster body is still visible
            .gfxID = 0X03008A2,
            .frameCount = 7,
        },
    },
},

{   .name = u8"霸王教主_火刃",
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X0300708,
            .frameCount = 7, // skip last 3 frames
            .gfxDirType = 8,
        },
    },
},

{   .name = u8"霸王教主_野蛮冲撞",
    .castRange
    {
        .type = CRT_DIR,
        .distance = 5,
    },
},

{   .name = u8"霸王教主_死亡特效",
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着", // may also make it as 固定, magic of type 附着 only get draw when monster body is still visible
            .gfxID = 0X0300762,
            .frameCount = 18,
            .gfxDirType = 1,
        },
    },
},

{   .name = u8"潘夜右护卫_死亡特效",
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X040005A,
            .frameCount = 5,
            .gfxDirType = 1,
        },
    },
},

{   .name = u8"潘夜右护卫_电魔杖",
    .castRange
    {
        .type = CRT_DIR,
        .distance = 1,
    },

    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X0400003,
            .frameCount = 3,
            .gfxDirType = 8,
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X010033E,
            .frameCount = 6,
            .gfxDirType = 1,
        },
    },
},

{   .name = u8"潘夜右护卫_雷电术",
    .elem = u8"雷",

    .castRange
    {
        .type = CRT_LIMITED,
        .distance = 10,
    },

    .gfxList
    {
        {
            .ref
            {
                .name = u8"魔法特效_雷电术",
                .stage = u8"运行",
                .modColor = colorf::YELLOW + colorf::A_SHF(127),
            },
        },
    },
},

{   .name = u8"潘夜左护卫_死亡特效",
    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X04000C8,
            .frameCount = 5,
            .gfxDirType = 1,
        },
    },
},

{   .name = u8"潘夜左护卫_火魔杖",
    .castRange
    {
        .type = CRT_DIR,
        .distance = 1,
    },

    .gfxList
    {
        {
            .stage = u8"运行",
            .type  = u8"附着",
            .gfxID = 0X0400071,
            .frameCount = 3,
            .gfxDirType = 8,
        },

        {
            .stage = u8"裂解",
            .type  = u8"附着",
            .gfxID = 0X0100316,
            .frameCount = 6,
            .gfxDirType = 1,
        },
    },
},

{   .name = u8"潘夜左护卫_火球术",
    .elem = u8"火",

    .castRange
    {
        .type = CRT_LIMITED,
        .distance = 8,
    },

    .gfxList
    {
        {
            .ref
            {
                .name = u8"魔法特效_火球术",
                .stage = u8"运行",
            },
        },

        {
            .ref
            {
                .name = u8"魔法特效_火球术",
                .stage = u8"裂解",
            },
        },
    },
},
