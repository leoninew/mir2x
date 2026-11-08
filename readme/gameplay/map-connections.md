# 地图连接索引

本表按当前 `common/src/maprecord.inc` 机械整理，仅包含目标地图名称存在的静态连接，并按同一格后定义覆盖先定义选取入口。每对地图只列出一个未被后续有效记录覆盖的入口格和该记录落点；点击目标名称查看原始条目及其他入口。源文件和地图资源更新后本表需重新核对。

格式：**目标地图** `来源入口 → 目标落点`。静态连接不保证地形可达，可能受任务脚本、地图资源和实例影响；没有列出 NPC、GM 或任务专用传送。用法与实例见[地图连接与路线](map-routes.md)。

| 当前地图 | 目标地图及一个入口、落点 |
| --- | --- |
| 比奇县_0 | [边境城市_01](../../common/src/maprecord.inc#L8) `(333,776) → (564,69)`；[银杏山谷_02](../../common/src/maprecord.inc#L11) `(776,701) → (27,455)`；[道馆_1](../../common/src/maprecord.inc#L15) `(206,52) → (512,582)`；[毒蛇山谷_2](../../common/src/maprecord.inc#L19) `(643,15) → (159,369)`；[半兽洞穴1层_D001](../../common/src/maprecord.inc#L23) `(65,174) → (151,362)`；[废矿矿山入口_D401](../../common/src/maprecord.inc#L24) `(764,206) → (25,181)` |
| 边境城市_01 | [比奇县_0](../../common/src/maprecord.inc#L33) `(563,68) → (332,775)`；[志善屋_01_001](../../common/src/maprecord.inc#L36) `(447,246) → (20,21)`；[沙漠_71](../../common/src/maprecord.inc#L37) `(86,405) → (379,36)`；[东部石矿_D1405](../../common/src/maprecord.inc#L44) `(497,86) → (86,365)`；[北部石矿1层_D1411](../../common/src/maprecord.inc#L45) `(355,65) → (92,387)`；[西部石矿1层_D1421](../../common/src/maprecord.inc#L46) `(294,153) → (92,387)` |
| 沙巴克城堡_0150 | [沙巴克城_3](../../common/src/maprecord.inc#L53) `(15,20) → (194,144)` |
| 沙巴克城楼_0157 | [祖玛神殿_31](../../common/src/maprecord.inc#L62) `(8,26) → (173,130)`；[祖玛神殿1层_D501](../../common/src/maprecord.inc#L64) `(20,5) → (166,216)` |
| 祖玛神殿大厅_0157 | [祖玛神殿_31](../../common/src/maprecord.inc#L73) `(8,26) → (173,130)`；[祖玛神殿1层_D501](../../common/src/maprecord.inc#L75) `(20,5) → (166,216)` |
| 志善屋_01_001 | [边境城市_01](../../common/src/maprecord.inc#L83) `(19,22) → (446,247)` |
| 银杏山谷_02 | [比奇县_0](../../common/src/maprecord.inc#L98) `(26,455) → (775,701)`；[天然洞穴1层_D011](../../common/src/maprecord.inc#L102) `(483,322) → (154,367)`；[东部银杏废矿1层_D1301](../../common/src/maprecord.inc#L103) `(346,120) → (358,378)`；[北部银杏废矿1层_D1311](../../common/src/maprecord.inc#L104) `(281,73) → (358,378)`；[西部银杏废矿1层_D1321](../../common/src/maprecord.inc#L105) `(129,157) → (358,378)` |
| 试练场_02_002 | [试练场_02_003](../../common/src/maprecord.inc#L115) `(90,15) → (22,83)` |
| 比奇县_0_003 | [比奇县_0](../../common/src/maprecord.inc#L157) `(24,31) → (514,391)` |
| 道馆_1 | [比奇县_0](../../common/src/maprecord.inc#L193) `(513,583) → (207,53)`；[灌木林_12](../../common/src/maprecord.inc#L197) `(403,30) → (121,467)`；[武器仓库_1_001](../../common/src/maprecord.inc#L200) `(429,120) → (6,17)`；[本馆_1_002](../../common/src/maprecord.inc#L201) `(429,96) → (10,17)`；[洗衣住居_1_003](../../common/src/maprecord.inc#L202) `(415,133) → (8,11)`；[书房_1_004](../../common/src/maprecord.inc#L203) `(415,96) → (11,15)`；[物品研究所_1_005](../../common/src/maprecord.inc#L204) `(367,134) → (11,15)`；[药剂师住居_1_006](../../common/src/maprecord.inc#L205) `(388,113) → (7,15)`；[仓库_1_007](../../common/src/maprecord.inc#L206) `(394,169) → (6,17)`；[无名老人隐居地_1_009](../../common/src/maprecord.inc#L207) `(185,65) → (41,137)`；[失乐园_9](../../common/src/maprecord.inc#L210) `(18,564) → (746,36)`；[沃玛神殿入口_D021](../../common/src/maprecord.inc#L215) `(54,369) → (76,75)` |
| 灌木林_12 | [道馆_1](../../common/src/maprecord.inc#L225) `(122,468) → (404,31)`；[灌木林_122](../../common/src/maprecord.inc#L227) `(362,394) → (24,365)`；[灌木林_125](../../common/src/maprecord.inc#L231) `(202,52) → (120,370)`；[赤月山谷1层_D1001](../../common/src/maprecord.inc#L236) `(334,52) → (269,384)`；[赤月山谷1层_D1011](../../common/src/maprecord.inc#L237) `(110,31) → (52,286)`；[赤月山谷1层_D1021](../../common/src/maprecord.inc#L238) `(46,143) → (288,287)` |
| 灌木林_122 | [灌木林_12](../../common/src/maprecord.inc#L252) `(20,363) → (363,390)`；[灌木林_125](../../common/src/maprecord.inc#L257) `(250,48) → (324,79)`；[沙巴克城_3](../../common/src/maprecord.inc#L263) `(340,369) → (32,268)` |
| 灌木林_125 | [灌木林_12](../../common/src/maprecord.inc#L286) `(121,371) → (203,53)`；[灌木林_122](../../common/src/maprecord.inc#L291) `(320,73) → (254,54)` |
| 武器仓库_1_001 | [道馆_1](../../common/src/maprecord.inc#L303) `(5,18) → (428,121)` |
| 本馆_1_002 | [道馆_1](../../common/src/maprecord.inc#L310) `(9,18) → (428,97)` |
| 洗衣住居_1_003 | [道馆_1](../../common/src/maprecord.inc#L317) `(7,12) → (414,134)` |
| 书房_1_004 | [道馆_1](../../common/src/maprecord.inc#L324) `(12,16) → (415,97)` |
| 物品研究所_1_005 | [道馆_1](../../common/src/maprecord.inc#L331) `(12,16) → (368,135)` |
| 药剂师住居_1_006 | [道馆_1](../../common/src/maprecord.inc#L338) `(8,16) → (389,114)` |
| 仓库_1_007 | [道馆_1](../../common/src/maprecord.inc#L345) `(5,18) → (393,170)` |
| 无名老人隐居地_1_009 | [道馆_1](../../common/src/maprecord.inc#L355) `(43,138) → (186,67)` |
| 试练场_1_010 | [试练场_1_011](../../common/src/maprecord.inc#L364) `(90,15) → (22,83)` |
| 困魔咒空间_1_015 | [困魔咒空间_1_016](../../common/src/maprecord.inc#L385) `(28,33) → (10,17)`；[沃玛神殿2层_D023](../../common/src/maprecord.inc#L388) `(9,15) → (371,364)` |
| 困魔咒空间_1_016 | [困魔咒空间_1_015](../../common/src/maprecord.inc#L395) `(9,15) → (28,31)`；[困魔咒空间_1_017](../../common/src/maprecord.inc#L396) `(28,33) → (10,17)` |
| 困魔咒空间_1_017 | [困魔咒空间_1_016](../../common/src/maprecord.inc#L405) `(9,15) → (28,31)`；[困魔咒空间_1_018](../../common/src/maprecord.inc#L406) `(28,33) → (10,17)` |
| 困魔咒空间_1_018 | [困魔咒空间_1_017](../../common/src/maprecord.inc#L415) `(9,15) → (28,31)`；[困魔咒空间_1_019](../../common/src/maprecord.inc#L416) `(28,33) → (13,17)` |
| 困魔咒空间_1_019 | [困魔咒空间_1_018](../../common/src/maprecord.inc#L428) `(12,15) → (28,31)`；[沃玛神殿2层_D023](../../common/src/maprecord.inc#L430) `(38,41) → (371,364)` |
| 蜈蚣洞穴_1_020 | [蜈蚣洞穴_1_021](../../common/src/maprecord.inc#L439) `(172,173) → (24,40)`；[绝命谷3层_D6004](../../common/src/maprecord.inc#L440) `(186,17) → (41,264)` |
| 蜈蚣洞穴_1_021 | [蜈蚣洞穴_1_020](../../common/src/maprecord.inc#L448) `(22,42) → (169,172)`；[蜈蚣洞穴_1_022](../../common/src/maprecord.inc#L450) `(10,12) → (163,30)` |
| 蜈蚣洞穴_1_022 | [蜈蚣洞穴_1_021](../../common/src/maprecord.inc#L458) `(165,36) → (12,14)`；[蜈蚣洞穴_1_023](../../common/src/maprecord.inc#L460) `(11,12) → (40,36)` |
| 蜈蚣洞穴_1_023 | [蜈蚣洞穴_1_022](../../common/src/maprecord.inc#L468) `(41,40) → (13,13)` |
| 毒蛇山谷_2 | [比奇县_0](../../common/src/maprecord.inc#L478) `(158,370) → (643,16)`；[沙巴克城_3](../../common/src/maprecord.inc#L482) `(118,55) → (364,571)`；[祖玛神殿_31](../../common/src/maprecord.inc#L486) `(256,35) → (122,258)`；[矿山1层_D421](../../common/src/maprecord.inc#L490) `(366,200) → (30,374)`；[南部矿山_D431](../../common/src/maprecord.inc#L491) `(342,137) → (25,181)`；[南部矿山1层_D451](../../common/src/maprecord.inc#L492) `(350,261) → (30,374)` |
| 沙巴克城_3 | [沙巴克城堡_0150](../../common/src/maprecord.inc#L501) `(194,144) → (14,19)`；[灌木林_122](../../common/src/maprecord.inc#L503) `(31,268) → (339,368)`；[毒蛇山谷_2](../../common/src/maprecord.inc#L508) `(363,572) → (118,56)`；[祖玛神殿_31](../../common/src/maprecord.inc#L513) `(349,38) → (24,50)` |
| 祖玛神殿_31 | [祖玛神殿大厅_0157](../../common/src/maprecord.inc#L526) `(174,128) → (9,26)`；[毒蛇山谷_2](../../common/src/maprecord.inc#L527) `(119,256) → (353,34)`；[沙巴克城_3](../../common/src/maprecord.inc#L532) `(23,51) → (348,39)` |
| 绿洲_4 | [诺玛村庄_41](../../common/src/maprecord.inc#L546) `(27,231) → (380,345)`；[沙漠_44](../../common/src/maprecord.inc#L550) `(68,466) → (370,117)`；[武器店_4_001](../../common/src/maprecord.inc#L556) `(449,73) → (13,13)`；[民家_4_002](../../common/src/maprecord.inc#L557) `(448,93) → (8,15)`；[中药商_4_003](../../common/src/maprecord.inc#L559) `(433,72) → (13,13)`；[布匹店_4_004](../../common/src/maprecord.inc#L560) `(462,89) → (8,13)`；[占卜屋_4_005](../../common/src/maprecord.inc#L561) `(476,57) → (11,26)`；[沙漠土城_5](../../common/src/maprecord.inc#L562) `(422,547) → (18,207)`；[沙漠_6](../../common/src/maprecord.inc#L582) `(180,778) → (178,80)`；[北部蚂蚁洞穴1层1_D8102](../../common/src/maprecord.inc#L595) `(375,119) → (179,188)`；[东部蚂蚁洞穴1层1_D8202](../../common/src/maprecord.inc#L596) `(476,152) → (179,188)` |
| 诺玛村庄_41 | [绿洲_4](../../common/src/maprecord.inc#L632) `(381,345) → (28,231)`；[沙漠_42](../../common/src/maprecord.inc#L637) `(29,141) → (371,328)`；[诺玛遗址1层_D1601](../../common/src/maprecord.inc#L642) `(115,72) → (38,148)` |
| 沙漠_42 | [诺玛村庄_41](../../common/src/maprecord.inc#L657) `(372,328) → (30,141)`；[沙漠_43](../../common/src/maprecord.inc#L662) `(34,189) → (332,65)` |
| 沙漠_43 | [沙漠_42](../../common/src/maprecord.inc#L675) `(333,64) → (35,188)`；[沙漠_44](../../common/src/maprecord.inc#L680) `(41,204) → (333,333)` |
| 沙漠_44 | [绿洲_4](../../common/src/maprecord.inc#L694) `(371,117) → (69,466)`；[沙漠_43](../../common/src/maprecord.inc#L700) `(334,333) → (42,204)` |
| 武器店_4_001 | [绿洲_4](../../common/src/maprecord.inc#L712) `(14,14) → (450,74)` |
| 民家_4_002 | [绿洲_4](../../common/src/maprecord.inc#L719) `(7,16) → (447,94)` |
| 中药商_4_003 | [绿洲_4](../../common/src/maprecord.inc#L727) `(14,14) → (434,73)` |
| 布匹店_4_004 | [绿洲_4](../../common/src/maprecord.inc#L734) `(7,14) → (461,90)` |
| 占卜屋_4_005 | [绿洲_4](../../common/src/maprecord.inc#L741) `(10,27) → (475,58)` |
| 沙漠土城_5 | [绿洲_4](../../common/src/maprecord.inc#L750) `(17,208) → (421,548)`；[贵族屋_5_0011](../../common/src/maprecord.inc#L770) `(236,161) → (24,37)`；[贵族屋_5_0012](../../common/src/maprecord.inc#L773) `(240,194) → (18,23)`；[贵族屋_5_0013](../../common/src/maprecord.inc#L774) `(206,169) → (17,47)`；[武器商_5_002](../../common/src/maprecord.inc#L775) `(190,249) → (13,17)`；[药房_5_003](../../common/src/maprecord.inc#L776) `(193,231) → (6,21)`；[服饰店_5_004](../../common/src/maprecord.inc#L778) `(179,278) → (9,21)`；[饰品店_5_005](../../common/src/maprecord.inc#L779) `(203,241) → (6,11)`；[仓库_5_006](../../common/src/maprecord.inc#L780) `(159,267) → (8,23)` |
| 贵族屋_5_0011 | [沙漠土城_5](../../common/src/maprecord.inc#L788) `(23,38) → (235,161)` |
| 贵族屋_5_0012 | [沙漠土城_5](../../common/src/maprecord.inc#L797) `(17,24) → (239,195)` |
| 贵族屋_5_0013 | [沙漠土城_5](../../common/src/maprecord.inc#L804) `(18,48) → (207,170)` |
| 武器商_5_002 | [沙漠土城_5](../../common/src/maprecord.inc#L811) `(14,18) → (191,250)` |
| 药房_5_003 | [沙漠土城_5](../../common/src/maprecord.inc#L818) `(6,22) → (194,232)` |
| 服饰店_5_004 | [沙漠土城_5](../../common/src/maprecord.inc#L826) `(10,22) → (179,279)` |
| 饰品店_5_005 | [沙漠土城_5](../../common/src/maprecord.inc#L833) `(5,12) → (203,242)` |
| 仓库_5_006 | [沙漠土城_5](../../common/src/maprecord.inc#L840) `(7,24) → (158,267)` |
| 沙漠_6 | [绿洲_4](../../common/src/maprecord.inc#L850) `(179,79) → (181,777)`；[沙漠_78](../../common/src/maprecord.inc#L863) `(767,559) → (37,110)`；[南部蚂蚁洞穴1层1_D8002](../../common/src/maprecord.inc#L868) `(291,719) → (179,188)` |
| 沙漠_71 | [边境城市_01](../../common/src/maprecord.inc#L877) `(380,36) → (87,405)`；[沙漠_72](../../common/src/maprecord.inc#L884) `(8,96) → (378,45)` |
| 沙漠_72 | [沙漠_71](../../common/src/maprecord.inc#L899) `(379,45) → (9,96)`；[沙漠_73](../../common/src/maprecord.inc#L906) `(20,62) → (350,144)` |
| 沙漠_73 | [沙漠_72](../../common/src/maprecord.inc#L921) `(350,145) → (21,63)`；[盟重县_74](../../common/src/maprecord.inc#L929) `(36,110) → (579,295)` |
| 盟重县_74 | [沙漠_73](../../common/src/maprecord.inc#L942) `(578,297) → (37,112)`；[石阁庙_75](../../common/src/maprecord.inc#L950) `(20,312) → (378,220)`；[绝命谷1层南部_D60011](../../common/src/maprecord.inc#L957) `(334,72) → (207,237)`；[万年谷_D6011](../../common/src/maprecord.inc#L958) `(78,227) → (159,168)`；[虫峡谷1层_D6021](../../common/src/maprecord.inc#L959) `(132,503) → (42,167)` |
| 石阁庙_75 | [盟重县_74](../../common/src/maprecord.inc#L968) `(378,221) → (21,313)`；[沙漠_76](../../common/src/maprecord.inc#L976) `(20,212) → (350,144)`；[石阁入口_D7101](../../common/src/maprecord.inc#L983) `(202,61) → (27,22)` |
| 沙漠_76 | [石阁庙_75](../../common/src/maprecord.inc#L992) `(350,145) → (21,213)`；[沙漠_77](../../common/src/maprecord.inc#L1000) `(36,110) → (351,143)` |
| 沙漠_77 | [沙漠_76](../../common/src/maprecord.inc#L1013) `(350,145) → (36,112)`；[沙漠_78](../../common/src/maprecord.inc#L1021) `(36,110) → (351,143)` |
| 沙漠_78 | [沙漠_6](../../common/src/maprecord.inc#L1034) `(36,110) → (766,559)`；[沙漠_77](../../common/src/maprecord.inc#L1039) `(350,145) → (36,112)` |
| 潘夜岛_8 | [潘夜神殿1层_D1101](../../common/src/maprecord.inc#L1055) `(692,358) → (135,180)`；[潘夜石窟1层_D1211](../../common/src/maprecord.inc#L1056) `(416,553) → (128,172)` |
| 流放岛_81 | [罪孽洞穴1层_D1201](../../common/src/maprecord.inc#L1065) `(156,135) → (19,147)` |
| 失乐园_9 | [道馆_1](../../common/src/maprecord.inc#L1074) `(747,35) → (19,563)`；[真天宫地下1层_D1500](../../common/src/maprecord.inc#L1079) `(388,305) → (172,224)`；[黑度宫1层_D1510](../../common/src/maprecord.inc#L1081) `(208,91) → (14,45)`；[南部蚂蚁洞穴1层1_D8002](../../common/src/maprecord.inc#L1082) `(732,674) → (40,175)` |
| 半兽洞穴1层_D001 | [比奇县_0](../../common/src/maprecord.inc#L1274) `(148,366) → (64,175)`；[半兽勇士洞_D001_001](../../common/src/maprecord.inc#L1276) `(303,65) → (8,10)`；[半兽洞穴2层_D002](../../common/src/maprecord.inc#L1277) `(30,328) → (34,323)` |
| 半兽勇士洞_D001_001 | [半兽洞穴1层_D001](../../common/src/maprecord.inc#L1286) `(7,9) → (302,66)` |
| 半兽洞穴2层_D002 | [半兽洞穴1层_D001](../../common/src/maprecord.inc#L1295) `(34,322) → (30,329)`；[骷髅精灵洞_D002_001](../../common/src/maprecord.inc#L1298) `(255,281) → (8,10)`；[半兽洞穴3层_D003](../../common/src/maprecord.inc#L1299) `(117,95) → (106,83)`；[奸商_DM001](../../common/src/maprecord.inc#L1300) `(191,231) → (5,10)`；[王陵_DM002](../../common/src/maprecord.inc#L1301) `(225,175) → (8,28)` |
| 骷髅精灵洞_D002_001 | [半兽洞穴2层_D002](../../common/src/maprecord.inc#L1308) `(7,9) → (254,282)` |
| 半兽洞穴3层_D003 | [半兽洞穴2层_D002](../../common/src/maprecord.inc#L1317) `(106,82) → (117,96)`；[半兽天然洞穴_E001](../../common/src/maprecord.inc#L1318) `(78,229) → (9,14)`；[连接通路_E402](../../common/src/maprecord.inc#L1319) `(290,293) → (13,73)` |
| 天然洞穴1层_D011 | [银杏山谷_02](../../common/src/maprecord.inc#L1328) `(155,367) → (481,324)`；[天然洞穴2层_D012](../../common/src/maprecord.inc#L1329) `(127,201) → (77,198)` |
| 天然洞穴2层_D012 | [天然洞穴1层_D011](../../common/src/maprecord.inc#L1339) `(77,197) → (127,202)`；[天然洞穴2层_D012_001](../../common/src/maprecord.inc#L1341) `(229,355) → (9,8)`；[洞穴奸商_DM011](../../common/src/maprecord.inc#L1342) `(189,144) → (5,10)`；[半兽天然洞穴_E002](../../common/src/maprecord.inc#L1343) `(377,64) → (35,82)` |
| 天然洞穴2层_D012_001 | [天然洞穴2层_D012](../../common/src/maprecord.inc#L1350) `(8,7) → (228,356)` |
| 沃玛神殿入口_D021 | [道馆_1](../../common/src/maprecord.inc#L1359) `(77,75) → (55,371)`；[沃玛神殿1层_D032](../../common/src/maprecord.inc#L1363) `(47,37) → (338,356)`；[沃玛神殿1层_D042](../../common/src/maprecord.inc#L1364) `(34,50) → (338,356)`；[沃玛神殿1层_D052](../../common/src/maprecord.inc#L1365) `(26,26) → (339,355)` |
| 沃玛神殿1层_D022 | [沃玛神殿入口_D021](../../common/src/maprecord.inc#L1375) `(338,354) → (25,25)`；[沃玛神殿2层_D023](../../common/src/maprecord.inc#L1377) `(251,206) → (199,194)` |
| 沃玛神殿1层_D022_001 | [沃玛神殿1层_D022](../../common/src/maprecord.inc#L1385) `(63,74) → (145,112)` |
| 沃玛神殿2层_D023 | [困魔咒空间_1_015](../../common/src/maprecord.inc#L1397) `(371,366) → (10,17)`；[沃玛神殿1层_D022](../../common/src/maprecord.inc#L1399) `(198,193) → (250,205)`；[沃玛神殿师徒关_D023_001](../../common/src/maprecord.inc#L1401) `(67,47) → (13,18)`；[沃玛神殿_D024](../../common/src/maprecord.inc#L1402) `(52,366) → (16,19)` |
| 沃玛神殿师徒关_D023_001 | [沃玛神殿2层_D023](../../common/src/maprecord.inc#L1409) `(12,17) → (65,47)` |
| 沃玛神殿师徒关_D023_002 | [沃玛神殿2层_D023](../../common/src/maprecord.inc#L1416) `(12,17) → (65,47)` |
| 沃玛神殿_D024 | [沃玛神殿2层_D033](../../common/src/maprecord.inc#L1426) `(46,28) → (51,365)`；[沃玛神殿2层_D043](../../common/src/maprecord.inc#L1427) `(28,46) → (51,365)`；[沃玛神殿2层_D053](../../common/src/maprecord.inc#L1428) `(16,18) → (51,365)` |
| 沃玛神殿1层_D032 | [沃玛神殿入口_D021](../../common/src/maprecord.inc#L1437) `(338,354) → (46,36)`；[沃玛神殿2层_D033](../../common/src/maprecord.inc#L1438) `(251,206) → (199,194)` |
| 沃玛神殿2层_D033 | [沃玛神殿_D024](../../common/src/maprecord.inc#L1448) `(52,366) → (47,29)`；[沃玛神殿1层_D032](../../common/src/maprecord.inc#L1449) `(198,193) → (250,205)` |
| 沃玛神殿1层_D042 | [沃玛神殿入口_D021](../../common/src/maprecord.inc#L1459) `(338,354) → (33,49)`；[沃玛神殿2层_D043](../../common/src/maprecord.inc#L1460) `(251,206) → (199,194)` |
| 沃玛神殿2层_D043 | [沃玛神殿_D024](../../common/src/maprecord.inc#L1470) `(52,366) → (29,47)`；[沃玛神殿1层_D042](../../common/src/maprecord.inc#L1471) `(198,193) → (250,205)` |
| 沃玛神殿1层_D052 | [沃玛神殿入口_D021](../../common/src/maprecord.inc#L1481) `(338,354) → (25,25)`；[沃玛神殿2层_D053](../../common/src/maprecord.inc#L1483) `(251,206) → (199,194)` |
| 沃玛神殿2层_D053 | [沃玛神殿_D024](../../common/src/maprecord.inc#L1493) `(52,366) → (16,19)`；[沃玛神殿1层_D052](../../common/src/maprecord.inc#L1494) `(198,193) → (250,205)` |
| 赤月山谷1层_D1001 | [灌木林_12](../../common/src/maprecord.inc#L1504) `(270,385) → (333,53)`；[赤月山谷2层_D1002](../../common/src/maprecord.inc#L1505) `(123,24) → (62,288)` |
| 赤月山谷2层_D1002 | [赤月山谷1层_D1001](../../common/src/maprecord.inc#L1523) `(61,289) → (122,25)`；[赤月山谷3层_D10031](../../common/src/maprecord.inc#L1524) `(13,26) → (289,150)`；[赤月山谷3层_D10032](../../common/src/maprecord.inc#L1525) `(289,138) → (10,155)` |
| 赤月山谷3层_D10031 | [赤月山谷2层_D1002](../../common/src/maprecord.inc#L1534) `(290,151) → (14,27)` |
| 赤月山谷3层_D10032 | [赤月山谷2层_D1002](../../common/src/maprecord.inc#L1543) `(9,156) → (288,139)`；[赤月山谷4层_D1004](../../common/src/maprecord.inc#L1544) `(201,148) → (171,88)` |
| 赤月山谷4层_D1004 | [赤月山谷3层_D10031](../../common/src/maprecord.inc#L1553) `(121,38) → (150,104)`；[赤月山谷3层_D10032](../../common/src/maprecord.inc#L1554) `(170,89) → (200,149)`；[赤月山谷5层_D10051](../../common/src/maprecord.inc#L1555) `(146,110) → (48,188)`；[赤月山谷5层_D10052](../../common/src/maprecord.inc#L1556) `(217,133) → (97,184)` |
| 赤月山谷5层_D10051 | [赤月山谷4层_D1004](../../common/src/maprecord.inc#L1565) `(47,189) → (145,111)`；[未命名地图_D10053](../../common/src/maprecord.inc#L1566) `(11,27) → (40,90)`；[未命名地图_D10061](../../common/src/maprecord.inc#L1567) `(178,53) → (20,25)` |
| 赤月山谷5层_D10052 | [赤月山谷4层_D1004](../../common/src/maprecord.inc#L1576) `(98,185) → (216,132)`；[未命名地图_D10054](../../common/src/maprecord.inc#L1577) `(183,178) → (79,13)`；[未命名地图_D10062](../../common/src/maprecord.inc#L1578) `(140,23) → (12,24)` |
| 未命名地图_D10053 | [赤月山谷5层_D10051](../../common/src/maprecord.inc#L1586) `(41,91) → (11,27)` |
| 未命名地图_D10054 | [赤月山谷5层_D10052](../../common/src/maprecord.inc#L1594) `(78,12) → (182,177)` |
| 未命名地图_D10061 | [赤月山谷5层_D10051](../../common/src/maprecord.inc#L1601) `(21,26) → (179,54)` |
| 未命名地图_D10071 | [未命名地图_D10061](../../common/src/maprecord.inc#L1611) `(11,25) → (15,19)` |
| 赤月山谷1层_D1011 | [灌木林_12](../../common/src/maprecord.inc#L1620) `(53,287) → (109,32)`；[赤月山谷2层_D1012](../../common/src/maprecord.inc#L1621) `(290,76) → (14,143)` |
| 赤月山谷2层_D1012 | [赤月山谷1层_D1011](../../common/src/maprecord.inc#L1630) `(13,144) → (289,77)`；[赤月山谷3层_D10131](../../common/src/maprecord.inc#L1631) `(274,109) → (286,142)`；[赤月山谷3层_D10132](../../common/src/maprecord.inc#L1632) `(134,15) → (18,82)` |
| 赤月山谷3层_D10131 | [赤月山谷2层_D1012](../../common/src/maprecord.inc#L1641) `(287,141) → (275,108)`；[赤月山谷4层_D1014](../../common/src/maprecord.inc#L1642) `(11,99) → (198,290)` |
| 赤月山谷3层_D10132 | [赤月山谷2层_D1012](../../common/src/maprecord.inc#L1651) `(17,83) → (133,16)`；[赤月山谷4层_D1014](../../common/src/maprecord.inc#L1652) `(234,25) → (153,254)` |
| 赤月山谷4层_D1014 | [赤月山谷3层_D10131](../../common/src/maprecord.inc#L1661) `(199,291) → (12,100)`；[赤月山谷3层_D10132](../../common/src/maprecord.inc#L1662) `(152,255) → (233,26)`；[赤月山谷5层_D10151](../../common/src/maprecord.inc#L1663) `(208,21) → (89,51)`；[赤月山谷5层_D10152](../../common/src/maprecord.inc#L1664) `(91,12) → (50,190)` |
| 赤月山谷5层_D10151 | [赤月山谷4层_D1014](../../common/src/maprecord.inc#L1673) `(88,52) → (207,22)`；[赤月恶魔洞穴_D10162](../../common/src/maprecord.inc#L1674) `(152,16) → (12,24)` |
| 赤月山谷5层_D10152 | [赤月山谷4层_D1014](../../common/src/maprecord.inc#L1683) `(51,191) → (92,13)` |
| 赤月恶魔洞穴_D10162 | [赤月山谷5层_D10151](../../common/src/maprecord.inc#L1691) `(11,25) → (151,17)` |
| 赤月山谷1层_D1021 | [灌木林_12](../../common/src/maprecord.inc#L1700) `(289,288) → (45,144)`；[赤月山谷2层_D1022](../../common/src/maprecord.inc#L1701) `(148,12) → (32,244)` |
| 赤月山谷2层_D1022 | [赤月山谷1层_D1021](../../common/src/maprecord.inc#L1710) `(31,245) → (147,13)`；[赤月山谷3层_D10231](../../common/src/maprecord.inc#L1711) `(46,32) → (148,152)`；[赤月山谷3层_D10232](../../common/src/maprecord.inc#L1712) `(195,22) → (52,98)` |
| 赤月山谷3层_D10231 | [赤月山谷2层_D1022](../../common/src/maprecord.inc#L1721) `(149,153) → (47,33)`；[赤月山谷4层_D1024](../../common/src/maprecord.inc#L1722) `(49,11) → (194,279)` |
| 赤月山谷3层_D10232 | [赤月山谷2层_D1022](../../common/src/maprecord.inc#L1731) `(51,99) → (194,23)`；[赤月山谷4层_D1024](../../common/src/maprecord.inc#L1732) `(133,22) → (282,264)` |
| 赤月山谷4层_D1024 | [赤月山谷3层_D10231](../../common/src/maprecord.inc#L1741) `(195,280) → (50,12)`；[赤月山谷3层_D10232](../../common/src/maprecord.inc#L1742) `(283,265) → (134,23)`；[赤月山谷5层_D10251](../../common/src/maprecord.inc#L1743) `(284,51) → (11,58)`；[赤月山谷5层_D10252](../../common/src/maprecord.inc#L1744) `(115,13) → (182,143)` |
| 赤月山谷5层_D10251 | [赤月山谷4层_D1024](../../common/src/maprecord.inc#L1753) `(10,59) → (283,52)` |
| 赤月山谷5层_D10252 | [赤月山谷4层_D1024](../../common/src/maprecord.inc#L1762) `(183,144) → (116,14)`；[未命名地图_D10262](../../common/src/maprecord.inc#L1763) `(181,16) → (12,24)` |
| 潘夜神殿1层_D1101 | [潘夜岛_8](../../common/src/maprecord.inc#L1781) `(134,181) → (691,359)`；[潘夜神殿2层_D1102](../../common/src/maprecord.inc#L1782) `(222,228) → (141,141)` |
| 潘夜神殿2层_D1102 | [潘夜神殿1层_D1101](../../common/src/maprecord.inc#L1791) `(140,140) → (221,227)`；[潘夜神殿3层西部_D11031](../../common/src/maprecord.inc#L1792) `(255,254) → (207,333)`；[潘夜神殿3层东部_D11032](../../common/src/maprecord.inc#L1793) `(51,46) → (35,64)` |
| 潘夜神殿3层西部_D11031 | [潘夜神殿2层_D1102](../../common/src/maprecord.inc#L1802) `(206,332) → (254,253)` |
| 潘夜神殿3层东部_D11032 | [潘夜神殿2层_D1102](../../common/src/maprecord.inc#L1811) `(36,63) → (52,45)`；[潘夜神殿4层_D1104](../../common/src/maprecord.inc#L1812) `(62,36) → (106,94)` |
| 潘夜神殿4层_D1104 | [潘夜神殿3层东部_D11032](../../common/src/maprecord.inc#L1821) `(107,93) → (63,35)`；[潘夜神殿5层_D1105](../../common/src/maprecord.inc#L1822) `(157,152) → (105,216)` |
| 潘夜神殿5层_D1105 | [潘夜神殿4层_D1104](../../common/src/maprecord.inc#L1831) `(106,217) → (158,153)` |
| 潘夜神殿大厅_D1110 | [潘夜神殿4层_D1111](../../common/src/maprecord.inc#L1839) `(25,22) → (221,224)` |
| 潘夜神殿4层_D1111 | [潘夜神殿大厅_D1110](../../common/src/maprecord.inc#L1848) `(220,225) → (24,23)`；[潘夜神殿5层_D1112](../../common/src/maprecord.inc#L1849) `(111,112) → (156,157)` |
| 潘夜神殿5层_D1112 | [潘夜神殿4层_D1111](../../common/src/maprecord.inc#L1858) `(155,156) → (110,111)`；[潘夜神殿6层_D1113](../../common/src/maprecord.inc#L1859) `(373,378) → (35,27)` |
| 潘夜神殿6层_D1113 | [潘夜神殿5层_D1112](../../common/src/maprecord.inc#L1868) `(34,26) → (372,377)`；[潘夜神殿7层西部_D11141](../../common/src/maprecord.inc#L1869) `(376,50) → (40,108)`；[潘夜神殿7层东部_D11142](../../common/src/maprecord.inc#L1870) `(38,373) → (93,127)`；[潘夜神殿8层_D1115](../../common/src/maprecord.inc#L1871) `(375,369) → (379,382)` |
| 潘夜神殿7层西部_D11141 | [潘夜神殿6层_D1113](../../common/src/maprecord.inc#L1880) `(39,109) → (375,51)`；[潘夜神殿8层_D1115](../../common/src/maprecord.inc#L1881) `(125,63) → (384,19)` |
| 潘夜神殿7层东部_D11142 | [潘夜神殿6层_D1113](../../common/src/maprecord.inc#L1890) `(94,128) → (39,374)`；[潘夜神殿8层_D1115](../../common/src/maprecord.inc#L1891) `(25,30) → (35,382)` |
| 潘夜神殿8层_D1115 | [潘夜神殿6层_D1113](../../common/src/maprecord.inc#L1900) `(380,383) → (376,370)`；[潘夜神殿7层西部_D11141](../../common/src/maprecord.inc#L1901) `(385,18) → (126,62)`；[潘夜神殿7层东部_D11142](../../common/src/maprecord.inc#L1902) `(36,383) → (26,31)`；[潘夜神殿_D1116](../../common/src/maprecord.inc#L1903) `(200,199) → (90,14)` |
| 潘夜神殿_D1116 | [潘夜神殿8层_D1115](../../common/src/maprecord.inc#L1912) `(91,13) → (201,198)` |
| 罪孽洞穴1层_D1201 | [流放岛_81](../../common/src/maprecord.inc#L1921) `(18,148) → (155,136)`；[罪孽洞穴2层_D1202](../../common/src/maprecord.inc#L1922) `(17,23) → (179,182)` |
| 罪孽洞穴2层_D1202 | [罪孽洞穴1层_D1201](../../common/src/maprecord.inc#L1931) `(179,183) → (18,24)`；[罪孽洞穴3层西部_D12031](../../common/src/maprecord.inc#L1932) `(16,24) → (50,80)`；[罪孽洞穴3层东部_D12032](../../common/src/maprecord.inc#L1933) `(120,87) → (18,186)` |
| 罪孽洞穴3层西部_D12031 | [罪孽洞穴2层_D1202](../../common/src/maprecord.inc#L1942) `(51,81) → (17,25)`；[罪孽洞穴4层_D1204](../../common/src/maprecord.inc#L1943) `(91,168) → (172,39)` |
| 罪孽洞穴3层东部_D12032 | [罪孽洞穴2层_D1202](../../common/src/maprecord.inc#L1952) `(17,187) → (119,88)`；[罪孽洞穴4层_D1204](../../common/src/maprecord.inc#L1953) `(29,35) → (24,267)` |
| 罪孽洞穴4层_D1204 | [罪孽洞穴3层西部_D12031](../../common/src/maprecord.inc#L1962) `(173,40) → (92,169)`；[罪孽洞穴3层东部_D12032](../../common/src/maprecord.inc#L1963) `(23,268) → (28,36)`；[罪孽洞穴5层_D1205](../../common/src/maprecord.inc#L1964) `(265,253) → (75,201)` |
| 罪孽洞穴5层_D1205 | [罪孽洞穴4层_D1204](../../common/src/maprecord.inc#L1973) `(74,202) → (264,254)`；[未命名地图_D12122](../../common/src/maprecord.inc#L1974) `(203,148) → (25,36)` |
| 潘夜石窟1层_D1211 | [潘夜岛_8](../../common/src/maprecord.inc#L1983) `(127,173) → (415,554)`；[潘夜石窟2层西部_D12121](../../common/src/maprecord.inc#L1984) `(10,96) → (281,61)` |
| 潘夜石窟2层西部_D12121 | [潘夜石窟1层_D1211](../../common/src/maprecord.inc#L1992) `(282,62) → (11,97)`；[未命名地图_D12122](../../common/src/maprecord.inc#L1993) `(27,23) → (79,84)`；[潘夜石窟3层_D1213](../../common/src/maprecord.inc#L1994) `(281,258) → (268,275)` |
| 未命名地图_D12122 | [罪孽洞穴5层_D1205](../../common/src/maprecord.inc#L2002) `(24,35) → (202,147)`；[潘夜石窟2层西部_D12121](../../common/src/maprecord.inc#L2003) `(80,85) → (28,24)` |
| 潘夜石窟3层_D1213 | [潘夜石窟2层西部_D12121](../../common/src/maprecord.inc#L2012) `(267,276) → (280,259)`；[潘夜石窟4层_D1214](../../common/src/maprecord.inc#L2013) `(107,130) → (59,284)` |
| 潘夜石窟4层_D1214 | [潘夜石窟3层_D1213](../../common/src/maprecord.inc#L2022) `(59,285) → (106,131)`；[潘夜石窟5层_D1215](../../common/src/maprecord.inc#L2023) `(226,16) → (371,47)` |
| 潘夜石窟5层_D1215 | [潘夜石窟4层_D1214](../../common/src/maprecord.inc#L2032) `(371,48) → (227,17)` |
| 东部银杏废矿1层_D1301 | [银杏山谷_02](../../common/src/maprecord.inc#L2041) `(357,379) → (345,121)`；[东部银杏废矿2层_D1302](../../common/src/maprecord.inc#L2042) `(155,31) → (367,353)` |
| 东部银杏废矿2层_D1302 | [东部银杏废矿1层_D1301](../../common/src/maprecord.inc#L2051) `(368,354) → (156,32)`；[东部银杏废矿3层_D1303](../../common/src/maprecord.inc#L2052) `(119,23) → (49,362)` |
| 东部银杏废矿3层_D1303 | [东部银杏废矿2层_D1302](../../common/src/maprecord.inc#L2061) `(48,363) → (118,24)`；[东部银杏废矿4层_D1304](../../common/src/maprecord.inc#L2062) `(359,27) → (69,352)` |
| 东部银杏废矿4层_D1304 | [东部银杏废矿3层_D1303](../../common/src/maprecord.inc#L2071) `(68,353) → (358,28)`；[东部银杏废矿5层_D1305](../../common/src/maprecord.inc#L2072) `(347,37) → (360,354)` |
| 东部银杏废矿5层_D1305 | [东部银杏废矿4层_D1304](../../common/src/maprecord.inc#L2081) `(361,355) → (348,38)` |
| 北部银杏废矿1层_D1311 | [银杏山谷_02](../../common/src/maprecord.inc#L2090) `(357,379) → (280,74)`；[北部银杏废矿2层_D1312](../../common/src/maprecord.inc#L2091) `(155,31) → (367,353)` |
| 北部银杏废矿2层_D1312 | [北部银杏废矿1层_D1311](../../common/src/maprecord.inc#L2100) `(368,354) → (156,32)`；[北部银杏废矿3层_D1313](../../common/src/maprecord.inc#L2101) `(119,23) → (49,362)` |
| 北部银杏废矿3层_D1313 | [北部银杏废矿2层_D1312](../../common/src/maprecord.inc#L2110) `(48,363) → (118,24)`；[北部银杏废矿4层_D1314](../../common/src/maprecord.inc#L2111) `(359,27) → (69,352)` |
| 北部银杏废矿4层_D1314 | [北部银杏废矿3层_D1313](../../common/src/maprecord.inc#L2120) `(68,353) → (358,28)`；[北部银杏废矿5层_D1315](../../common/src/maprecord.inc#L2121) `(347,37) → (360,354)` |
| 北部银杏废矿5层_D1315 | [北部银杏废矿4层_D1314](../../common/src/maprecord.inc#L2130) `(361,355) → (348,38)` |
| 西部银杏废矿1层_D1321 | [银杏山谷_02](../../common/src/maprecord.inc#L2139) `(357,379) → (128,158)`；[西部银杏废矿2层_D1322](../../common/src/maprecord.inc#L2140) `(155,31) → (367,353)` |
| 西部银杏废矿2层_D1322 | [西部银杏废矿1层_D1321](../../common/src/maprecord.inc#L2149) `(368,354) → (156,32)`；[西部银杏废矿3层_D1323](../../common/src/maprecord.inc#L2150) `(119,23) → (49,362)` |
| 西部银杏废矿3层_D1323 | [西部银杏废矿2层_D1322](../../common/src/maprecord.inc#L2159) `(48,363) → (118,24)`；[西部银杏废矿4层_D1324](../../common/src/maprecord.inc#L2160) `(359,27) → (69,352)` |
| 西部银杏废矿4层_D1324 | [西部银杏废矿3层_D1323](../../common/src/maprecord.inc#L2169) `(68,353) → (358,28)`；[西部银杏废矿5层_D1325](../../common/src/maprecord.inc#L2170) `(347,37) → (360,354)` |
| 西部银杏废矿5层_D1325 | [西部银杏废矿4层_D1324](../../common/src/maprecord.inc#L2179) `(361,355) → (348,38)` |
| 东部石矿_D1405 | [边境城市_01](../../common/src/maprecord.inc#L2208) `(85,366) → (496,87)` |
| 北部石矿1层_D1411 | [边境城市_01](../../common/src/maprecord.inc#L2217) `(91,388) → (354,66)`；[北部石矿2层_D1412](../../common/src/maprecord.inc#L2218) `(347,47) → (314,373)` |
| 北部石矿2层_D1412 | [北部石矿1层_D1411](../../common/src/maprecord.inc#L2227) `(313,374) → (346,48)`；[北部石矿3层_D1413](../../common/src/maprecord.inc#L2228) `(35,354) → (315,354)` |
| 北部石矿3层_D1413 | [北部石矿2层_D1412](../../common/src/maprecord.inc#L2237) `(314,355) → (36,355)`；[北部石矿4层_D1414](../../common/src/maprecord.inc#L2238) `(101,44) → (50,355)` |
| 北部石矿4层_D1414 | [北部石矿3层_D1413](../../common/src/maprecord.inc#L2247) `(49,356) → (100,45)`；[北部石矿5层_D1415](../../common/src/maprecord.inc#L2248) `(376,61) → (86,365)` |
| 北部石矿5层_D1415 | [北部石矿4层_D1414](../../common/src/maprecord.inc#L2257) `(85,366) → (375,62)` |
| 西部石矿1层_D1421 | [边境城市_01](../../common/src/maprecord.inc#L2266) `(91,388) → (293,154)`；[西部石矿2层_D1422](../../common/src/maprecord.inc#L2267) `(347,47) → (314,373)` |
| 西部石矿2层_D1422 | [西部石矿1层_D1421](../../common/src/maprecord.inc#L2276) `(313,374) → (346,48)`；[西部石矿3层_D1423](../../common/src/maprecord.inc#L2277) `(35,354) → (315,354)` |
| 西部石矿3层_D1423 | [西部石矿2层_D1422](../../common/src/maprecord.inc#L2286) `(314,355) → (36,355)`；[西部石矿4层_D1424](../../common/src/maprecord.inc#L2287) `(101,44) → (50,355)` |
| 西部石矿4层_D1424 | [西部石矿3层_D1423](../../common/src/maprecord.inc#L2296) `(49,356) → (100,45)`；[西部石矿5层_D1425](../../common/src/maprecord.inc#L2297) `(376,61) → (86,365)` |
| 西部石矿5层_D1425 | [西部石矿4层_D1424](../../common/src/maprecord.inc#L2306) `(85,366) → (375,62)` |
| 真天宫地下1层_D1500 | [失乐园_9](../../common/src/maprecord.inc#L2315) `(171,225) → (387,306)`；[真天宫西馆1层_D15011](../../common/src/maprecord.inc#L2318) `(15,45) → (268,90)`；[真天宫南馆1层_D15012](../../common/src/maprecord.inc#L2319) `(27,369) → (240,27)`；[真天宫东馆1层_D15013](../../common/src/maprecord.inc#L2321) `(351,382) → (40,38)`；[真天宫北馆1层_D15014](../../common/src/maprecord.inc#L2323) `(355,41) → (119,198)` |
| 真天宫西馆1层_D15011 | [真天宫地下1层_D1500](../../common/src/maprecord.inc#L2332) `(268,92) → (16,46)`；[真天宫西馆2层_D15021](../../common/src/maprecord.inc#L2334) `(16,276) → (242,21)` |
| 真天宫南馆1层_D15012 | [真天宫地下1层_D1500](../../common/src/maprecord.inc#L2344) `(241,26) → (28,368)` |
| 真天宫东馆1层_D15013 | [真天宫地下1层_D1500](../../common/src/maprecord.inc#L2353) `(39,37) → (351,380)`；[真天宫东馆2层_D15023](../../common/src/maprecord.inc#L2354) `(149,152) → (160,152)` |
| 真天宫北馆1层_D15014 | [真天宫地下1层_D1500](../../common/src/maprecord.inc#L2363) `(118,199) → (354,42)`；[真天宫北馆2层_D15024](../../common/src/maprecord.inc#L2365) `(258,35) → (122,132)` |
| 真天宫西馆2层_D15021 | [真天宫西馆1层_D15011](../../common/src/maprecord.inc#L2374) `(243,20) → (17,275)`；[真天宫南馆2层_D15022](../../common/src/maprecord.inc#L2375) `(119,262) → (18,18)` |
| 真天宫南馆2层_D15022 | [真天宫西馆2层_D15021](../../common/src/maprecord.inc#L2385) `(17,17) → (119,260)`；[真天宫南馆3层_D15032](../../common/src/maprecord.inc#L2386) `(245,265) → (14,19)`；[黑度宫3层_D1512](../../common/src/maprecord.inc#L2388) `(165,62) → (183,168)` |
| 真天宫东馆2层_D15023 | [真天宫东馆1层_D15013](../../common/src/maprecord.inc#L2397) `(160,154) → (150,153)`；[真天宫东馆3层_D15033](../../common/src/maprecord.inc#L2399) `(258,276) → (30,34)` |
| 真天宫北馆2层_D15024 | [真天宫北馆1层_D15014](../../common/src/maprecord.inc#L2409) `(121,133) → (257,36)`；[真天宫北馆3层_D15034](../../common/src/maprecord.inc#L2411) `(217,285) → (267,36)` |
| 真天宫南馆3层_D15032 | [真天宫南馆2层_D15022](../../common/src/maprecord.inc#L2421) `(13,18) → (245,263)`；[真天宫东馆3层_D15033](../../common/src/maprecord.inc#L2422) `(269,238) → (66,204)` |
| 真天宫东馆3层_D15033 | [真天宫东馆2层_D15023](../../common/src/maprecord.inc#L2431) `(29,33) → (258,274)`；[真天宫南馆3层_D15032](../../common/src/maprecord.inc#L2432) `(65,205) → (268,239)`；[真天宫北馆4层_D15043](../../common/src/maprecord.inc#L2434) `(285,290) → (12,20)`；[黑度宫4层_D1513](../../common/src/maprecord.inc#L2436) `(281,53) → (14,123)` |
| 真天宫北馆3层_D15034 | [真天宫北馆2层_D15024](../../common/src/maprecord.inc#L2445) `(268,35) → (218,284)` |
| 真天宫北馆4层_D15043 | [真天宫东馆3层_D15033](../../common/src/maprecord.inc#L2454) `(11,19) → (285,288)`；[真天宫北馆4层_D15044](../../common/src/maprecord.inc#L2455) `(255,259) → (22,267)` |
| 真天宫北馆4层_D15044 | [真天宫北馆4层_D15043](../../common/src/maprecord.inc#L2464) `(21,268) → (254,260)`；[真天宫北馆5层_D15054](../../common/src/maprecord.inc#L2466) `(245,20) → (181,183)` |
| 真天宫北馆5层_D15054 | [真天宫北馆4层_D15044](../../common/src/maprecord.inc#L2475) `(181,185) → (246,21)`；[真天宫_D1506](../../common/src/maprecord.inc#L2477) `(285,288) → (44,63)` |
| 真天宫_D1506 | [真天宫北馆5层_D15054](../../common/src/maprecord.inc#L2485) `(45,64) → (286,289)` |
| 黑度宫1层_D1510 | [失乐园_9](../../common/src/maprecord.inc#L2494) `(13,46) → (207,92)`；[黑度宫2层_D15111](../../common/src/maprecord.inc#L2496) `(9,172) → (178,34)`；[黑度宫2层_D15112](../../common/src/maprecord.inc#L2498) `(189,188) → (95,59)` |
| 黑度宫2层_D15111 | [黑度宫1层_D1510](../../common/src/maprecord.inc#L2508) `(179,33) → (10,171)`；[黑度宫3层_D1512](../../common/src/maprecord.inc#L2509) `(31,186) → (38,23)` |
| 黑度宫2层_D15112 | [黑度宫1层_D1510](../../common/src/maprecord.inc#L2519) `(94,58) → (189,186)`；[黑度宫3层_D1512](../../common/src/maprecord.inc#L2520) `(30,181) → (186,19)` |
| 黑度宫3层_D1512 | [真天宫南馆2层_D15022](../../common/src/maprecord.inc#L2530) `(183,170) → (166,63)`；[黑度宫2层_D15111](../../common/src/maprecord.inc#L2532) `(39,22) → (32,185)`；[黑度宫2层_D15112](../../common/src/maprecord.inc#L2533) `(187,18) → (31,180)`；[黑度宫4层_D1513](../../common/src/maprecord.inc#L2534) `(15,186) → (180,28)` |
| 黑度宫4层_D1513 | [真天宫东馆3层_D15033](../../common/src/maprecord.inc#L2544) `(13,124) → (280,54)`；[黑度宫3层_D1512](../../common/src/maprecord.inc#L2546) `(181,27) → (16,185)` |
| 诺玛遗址1层_D1601 | [诺玛村庄_41](../../common/src/maprecord.inc#L2554) `(39,148) → (116,72)`；[诺玛遗址2层_D1602](../../common/src/maprecord.inc#L2559) `(206,202) → (194,200)` |
| 诺玛遗址2层_D1602 | [诺玛遗址1层_D1601](../../common/src/maprecord.inc#L2568) `(195,198) → (208,201)`；[诺玛遗址3层_D16031](../../common/src/maprecord.inc#L2569) `(383,204) → (357,368)`；[诺玛遗址3层_D16032](../../common/src/maprecord.inc#L2570) `(199,14) → (341,78)`；[诺玛遗址3层_D16033](../../common/src/maprecord.inc#L2571) `(12,194) → (42,54)`；[诺玛遗址3层_D16034](../../common/src/maprecord.inc#L2572) `(188,384) → (62,356)` |
| 诺玛遗址3层_D16031 | [诺玛遗址2层_D1602](../../common/src/maprecord.inc#L2580) `(356,367) → (382,204)`；[诺玛遗址4层_D1604](../../common/src/maprecord.inc#L2581) `(199,219) → (190,177)` |
| 诺玛遗址3层_D16032 | [诺玛遗址2层_D1602](../../common/src/maprecord.inc#L2589) `(340,78) → (198,15)`；[诺玛遗址4层_D1604](../../common/src/maprecord.inc#L2590) `(200,216) → (191,186)` |
| 诺玛遗址3层_D16033 | [诺玛遗址2层_D1602](../../common/src/maprecord.inc#L2598) `(43,54) → (13,195)`；[诺玛遗址4层_D1604](../../common/src/maprecord.inc#L2599) `(198,217) → (199,185)` |
| 诺玛遗址3层_D16034 | [诺玛遗址2层_D1602](../../common/src/maprecord.inc#L2607) `(63,355) → (189,384)`；[诺玛遗址4层_D1604](../../common/src/maprecord.inc#L2608) `(199,217) → (200,176)` |
| 诺玛遗址4层_D1604 | [诺玛遗址3层_D16031](../../common/src/maprecord.inc#L2616) `(191,177) → (200,220)`；[诺玛遗址3层_D16032](../../common/src/maprecord.inc#L2617) `(192,184) → (201,216)`；[诺玛遗址3层_D16033](../../common/src/maprecord.inc#L2618) `(198,184) → (197,217)`；[诺玛遗址3层_D16034](../../common/src/maprecord.inc#L2619) `(198,177) → (198,218)`；[诺玛遗址4层_D1604](../../common/src/maprecord.inc#L2620) `(23,194) → (136,246)`；[诺玛遗址5层_D1605](../../common/src/maprecord.inc#L2759) `(130,298) → (113,96)` |
| 诺玛遗址5层_D1605 | [诺玛遗址4层_D1604](../../common/src/maprecord.inc#L2769) `(88,158) → (335,198)`；[诺玛遗址5层_D1605](../../common/src/maprecord.inc#L2775) `(16,191) → (86,219)`；[诺玛勇士墓穴_D1606](../../common/src/maprecord.inc#L2974) `(327,213) → (38,50)` |
| 诺玛勇士墓穴_D1606 | [诺玛遗址5层_D1605](../../common/src/maprecord.inc#L2981) `(38,50) → (327,213)` |
| 废矿矿山入口_D401 | [比奇县_0](../../common/src/maprecord.inc#L3015) `(24,182) → (763,207)`；[尸王房_D401_001](../../common/src/maprecord.inc#L3016) `(80,172) → (15,15)`；[地下1层第一间房_D411](../../common/src/maprecord.inc#L3021) `(76,15) → (59,8)`；[地下1层第二间房_D413](../../common/src/maprecord.inc#L3022) `(179,109) → (11,8)` |
| 废矿东部洞穴_D402 | [尸王房_D402_001](../../common/src/maprecord.inc#L3034) `(22,105) → (15,15)`；[地下1层第二间房_D413](../../common/src/maprecord.inc#L3039) `(8,147) → (85,10)`；[地下1层第三间房_D414](../../common/src/maprecord.inc#L3040) `(195,81) → (15,78)` |
| 地下1层采矿所_D403 | [尸王房_D403_001](../../common/src/maprecord.inc#L3052) `(28,144) → (15,15)`；[天桥2_D412](../../common/src/maprecord.inc#L3057) `(121,191) → (55,24)`；[地下1层第三间房_D414](../../common/src/maprecord.inc#L3058) `(10,106) → (70,20)` |
| 地下2层采矿所_D404 | [尸王房_D404_001](../../common/src/maprecord.inc#L3070) `(78,80) → (15,15)`；[废矿矿山地下2层_D404_002](../../common/src/maprecord.inc#L3075) `(55,127) → (4,11)`；[天桥2_D412](../../common/src/maprecord.inc#L3076) `(129,42) → (52,53)`；[天桥3_D416](../../common/src/maprecord.inc#L3077) `(9,152) → (89,17)`；[连接通路_E402](../../common/src/maprecord.inc#L3078) `(152,19) → (70,15)` |
| 废矿矿山地下2层_D404_002 | [地下2层采矿所_D404](../../common/src/maprecord.inc#L3088) `(3,12) → (54,128)` |
| 矿石储藏所_D405 | [尸王房_D405_001](../../common/src/maprecord.inc#L3097) `(69,95) → (15,15)`；[天桥1_D415](../../common/src/maprecord.inc#L3102) `(11,148) → (83,17)`；[天桥3_D416](../../common/src/maprecord.inc#L3103) `(166,138) → (7,55)` |
| 废矿南部洞穴_D406 | [尸王房_D406_001](../../common/src/maprecord.inc#L3115) `(79,72) → (15,15)`；[地下1层第一间房_D411](../../common/src/maprecord.inc#L3120) `(23,15) → (94,48)`；[天桥1_D415](../../common/src/maprecord.inc#L3121) `(186,119) → (9,85)` |
| 地下1层第一间房_D411 | [废矿矿山入口_D401](../../common/src/maprecord.inc#L3133) `(60,7) → (77,14)`；[废矿南部洞穴_D406](../../common/src/maprecord.inc#L3134) `(95,49) → (24,16)` |
| 天桥2_D412 | [地下1层采矿所_D403](../../common/src/maprecord.inc#L3143) `(54,23) → (120,190)`；[地下2层采矿所_D404](../../common/src/maprecord.inc#L3144) `(53,52) → (130,42)` |
| 地下1层第二间房_D413 | [废矿矿山入口_D401](../../common/src/maprecord.inc#L3153) `(10,7) → (179,108)`；[废矿东部洞穴_D402](../../common/src/maprecord.inc#L3154) `(86,9) → (9,146)` |
| 地下1层第三间房_D414 | [废矿东部洞穴_D402](../../common/src/maprecord.inc#L3163) `(14,79) → (194,82)`；[地下1层采矿所_D403](../../common/src/maprecord.inc#L3164) `(71,19) → (11,105)` |
| 天桥1_D415 | [矿石储藏所_D405](../../common/src/maprecord.inc#L3173) `(84,16) → (12,147)`；[废矿南部洞穴_D406](../../common/src/maprecord.inc#L3174) `(8,86) → (185,120)` |
| 天桥3_D416 | [地下2层采矿所_D404](../../common/src/maprecord.inc#L3183) `(90,16) → (10,151)`；[矿石储藏所_D405](../../common/src/maprecord.inc#L3184) `(6,54) → (165,137)` |
| 矿山1层_D421 | [毒蛇山谷_2](../../common/src/maprecord.inc#L3196) `(30,375) → (365,201)`；[未命名地图_D421_001](../../common/src/maprecord.inc#L3197) `(80,202) → (15,15)`；[矿山2层_D422](../../common/src/maprecord.inc#L3202) `(361,19) → (22,41)` |
| 矿山2层_D422 | [矿山1层_D421](../../common/src/maprecord.inc#L3214) `(22,42) → (361,20)`；[未命名地图_D422_001](../../common/src/maprecord.inc#L3215) `(128,101) → (15,15)`；[矿山2层_D422_002](../../common/src/maprecord.inc#L3220) `(167,190) → (30,9)`；[竞技场_E601](../../common/src/maprecord.inc#L3221) `(272,312) → (135,19)` |
| 矿山2层_D422_002 | [矿山2层_D422](../../common/src/maprecord.inc#L3231) `(31,8) → (168,189)` |
| 北部矿山入口_D431 | [毒蛇山谷_2](../../common/src/maprecord.inc#L3243) `(24,182) → (341,138)`；[尸王房_D431_001](../../common/src/maprecord.inc#L3244) `(80,172) → (15,15)`；[北部地下1层第一间房_D441](../../common/src/maprecord.inc#L3249) `(76,15) → (59,8)`；[北部地下1层第二间房_D443](../../common/src/maprecord.inc#L3250) `(179,109) → (11,8)` |
| 南部矿山_D431 | [毒蛇山谷_2](../../common/src/maprecord.inc#L3259) `(24,182) → (341,138)`；[尸王房_D431_001](../../common/src/maprecord.inc#L3260) `(80,172) → (15,15)`；[北部地下1层第一间房_D441](../../common/src/maprecord.inc#L3265) `(76,15) → (59,8)`；[北部地下1层第二间房_D443](../../common/src/maprecord.inc#L3266) `(179,109) → (11,8)` |
| 北部矿山_D432 | [尸王房_D432_001](../../common/src/maprecord.inc#L3278) `(22,105) → (15,15)`；[北部地下1层第二间房_D443](../../common/src/maprecord.inc#L3283) `(8,147) → (85,10)`；[北部地下1层第三间房_D444](../../common/src/maprecord.inc#L3284) `(195,81) → (15,78)` |
| 北部矿山东面洞穴_D432 | [尸王房_D432_001](../../common/src/maprecord.inc#L3293) `(22,105) → (15,15)`；[北部地下1层第二间房_D443](../../common/src/maprecord.inc#L3298) `(8,147) → (85,10)`；[北部地下1层第三间房_D444](../../common/src/maprecord.inc#L3299) `(195,81) → (15,78)` |
| 北部地下1层采矿所_D433 | [尸王房_D433_001](../../common/src/maprecord.inc#L3311) `(28,144) → (15,15)`；[北部天桥2_D442](../../common/src/maprecord.inc#L3316) `(121,191) → (55,24)`；[北部地下1层第三间房_D444](../../common/src/maprecord.inc#L3317) `(10,106) → (70,20)` |
| 北部地下2层采矿所_D434 | [尸王房_D434_001](../../common/src/maprecord.inc#L3329) `(78,80) → (15,15)`；[北部天桥2_D442](../../common/src/maprecord.inc#L3334) `(129,42) → (52,53)`；[北部天桥3_D446](../../common/src/maprecord.inc#L3335) `(9,152) → (89,17)` |
| 北部矿石储藏所_D435 | [尸王房_D435_001](../../common/src/maprecord.inc#L3350) `(69,95) → (15,15)`；[北部天桥1_D445](../../common/src/maprecord.inc#L3355) `(11,148) → (83,17)`；[北部天桥3_D446](../../common/src/maprecord.inc#L3356) `(166,138) → (7,55)` |
| 北部废矿南面洞穴_D436 | [尸王房_D436_001](../../common/src/maprecord.inc#L3368) `(79,72) → (15,15)`；[北部地下1层第一间房_D441](../../common/src/maprecord.inc#L3373) `(23,15) → (94,48)`；[北部天桥1_D445](../../common/src/maprecord.inc#L3374) `(186,119) → (9,85)` |
| 北部地下1层第一间房_D441 | [南部矿山_D431](../../common/src/maprecord.inc#L3386) `(60,7) → (77,14)`；[北部废矿南面洞穴_D436](../../common/src/maprecord.inc#L3387) `(95,49) → (24,16)` |
| 北部天桥2_D442 | [北部地下1层采矿所_D433](../../common/src/maprecord.inc#L3396) `(54,23) → (120,190)`；[北部地下2层采矿所_D434](../../common/src/maprecord.inc#L3397) `(53,52) → (130,42)` |
| 北部地下1层第二间房_D443 | [南部矿山_D431](../../common/src/maprecord.inc#L3406) `(10,7) → (179,108)`；[北部矿山_D432](../../common/src/maprecord.inc#L3407) `(86,9) → (9,146)` |
| 北部地下1层第三间房_D444 | [北部矿山_D432](../../common/src/maprecord.inc#L3416) `(14,79) → (194,82)`；[北部地下1层采矿所_D433](../../common/src/maprecord.inc#L3417) `(71,19) → (11,105)` |
| 北部天桥1_D445 | [北部矿石储藏所_D435](../../common/src/maprecord.inc#L3426) `(84,16) → (12,147)`；[北部废矿南面洞穴_D436](../../common/src/maprecord.inc#L3427) `(8,86) → (185,120)` |
| 北部天桥3_D446 | [北部地下2层采矿所_D434](../../common/src/maprecord.inc#L3436) `(90,16) → (10,151)`；[北部矿石储藏所_D435](../../common/src/maprecord.inc#L3437) `(6,54) → (165,137)` |
| 南部矿山1层_D451 | [毒蛇山谷_2](../../common/src/maprecord.inc#L3446) `(30,375) → (349,262)`；[尸王房_D451_001](../../common/src/maprecord.inc#L3447) `(80,202) → (15,15)`；[南部矿山2层_D452](../../common/src/maprecord.inc#L3452) `(361,19) → (22,41)` |
| 南部矿山2层_D452 | [南部矿山1层_D451](../../common/src/maprecord.inc#L3464) `(22,42) → (361,20)`；[尸王房_D452_001](../../common/src/maprecord.inc#L3465) `(128,101) → (15,15)` |
| 祖玛神殿1层_D501 | [祖玛神殿大厅_0157](../../common/src/maprecord.inc#L3481) `(165,217) → (19,16)`；[祖玛神殿2层_D502](../../common/src/maprecord.inc#L3482) `(350,170) → (166,216)`；[祖玛神殿_D511](../../common/src/maprecord.inc#L3483) `(302,74) → (29,21)`；[祖玛神殿_D512](../../common/src/maprecord.inc#L3484) `(326,97) → (20,11)` |
| 祖玛神殿2层_D502 | [祖玛神殿1层_D501](../../common/src/maprecord.inc#L3493) `(165,217) → (350,171)`；[祖玛神殿3层_D503](../../common/src/maprecord.inc#L3494) `(112,231) → (166,216)`；[祖玛神殿_D513](../../common/src/maprecord.inc#L3495) `(302,74) → (29,21)`；[祖玛神殿_D514](../../common/src/maprecord.inc#L3496) `(326,97) → (20,11)` |
| 祖玛神殿3层_D503 | [祖玛神殿2层_D502](../../common/src/maprecord.inc#L3505) `(165,217) → (112,232)`；[祖玛神殿4层_D504](../../common/src/maprecord.inc#L3506) `(295,146) → (166,216)` |
| 祖玛神殿4层_D504 | [祖玛神殿3层_D503](../../common/src/maprecord.inc#L3515) `(165,217) → (295,147)`；[祖玛神殿5层_D505](../../common/src/maprecord.inc#L3516) `(51,173) → (96,101)` |
| 祖玛神殿5层_D505 | [祖玛神殿4层_D504](../../common/src/maprecord.inc#L3525) `(95,102) → (52,174)`；[祖玛神殿6层大厅_D5071](../../common/src/maprecord.inc#L3526) `(123,10) → (8,10)` |
| 祖玛神殿6层大厅_D5071 | [祖玛神殿5层_D505](../../common/src/maprecord.inc#L3571) `(7,9) → (122,11)`；[祖玛神殿6层_D5072](../../common/src/maprecord.inc#L3572) `(39,43) → (7,51)` |
| 祖玛神殿6层_D5072 | [祖玛神殿6层大厅_D5071](../../common/src/maprecord.inc#L3581) `(6,50) → (38,42)`；[祖玛神殿6层_D5073](../../common/src/maprecord.inc#L3582) `(90,20) → (10,69)` |
| 祖玛神殿6层_D5073 | [祖玛神殿6层_D5072](../../common/src/maprecord.inc#L3591) `(9,70) → (89,21)`；[祖玛神殿6层_D5074](../../common/src/maprecord.inc#L3592) `(60,9) → (47,90)` |
| 祖玛神殿6层_D5074 | [祖玛神殿6层_D5073](../../common/src/maprecord.inc#L3601) `(48,91) → (59,10)`；[祖玛教主宫廷_D515](../../common/src/maprecord.inc#L3602) `(77,15) → (14,31)` |
| 祖玛神殿_D511 | [祖玛神殿1层_D501](../../common/src/maprecord.inc#L3609) `(29,22) → (303,75)` |
| 祖玛神殿_D512 | [祖玛神殿1层_D501](../../common/src/maprecord.inc#L3616) `(19,11) → (325,96)` |
| 祖玛神殿_D513 | [祖玛神殿2层_D502](../../common/src/maprecord.inc#L3623) `(29,22) → (303,75)` |
| 祖玛神殿_D514 | [祖玛神殿2层_D502](../../common/src/maprecord.inc#L3630) `(19,11) → (325,96)` |
| 祖玛教主宫廷_D515 | [祖玛神殿6层_D5074](../../common/src/maprecord.inc#L3638) `(13,32) → (76,16)` |
| 绝命谷1层南部_D60011 | [盟重县_74](../../common/src/maprecord.inc#L3647) `(208,238) → (335,73)`；[石灯屋_D603](../../common/src/maprecord.inc#L3648) `(52,14) → (17,27)`；[奇岩石屋_D606](../../common/src/maprecord.inc#L3649) `(149,55) → (29,28)` |
| 绝命谷1层西部_D60012 | [绝命谷2层北部_D60022](../../common/src/maprecord.inc#L3658) `(45,22) → (172,61)`；[石灯屋_D603](../../common/src/maprecord.inc#L3659) `(110,86) → (31,9)`；[奇岩通路_D607](../../common/src/maprecord.inc#L3660) `(177,33) → (33,40)` |
| 绝命谷1层北部_D60013 | [紫水晶洞_D604](../../common/src/maprecord.inc#L3668) `(41,194) → (19,23)`；[奇岩石屋_D606](../../common/src/maprecord.inc#L3669) `(81,81) → (29,16)`；[奇岩通路_D607](../../common/src/maprecord.inc#L3670) `(185,82) → (55,38)` |
| 绝命谷2层西部_D60021 | [紫水晶洞_D604](../../common/src/maprecord.inc#L3679) `(149,132) → (18,32)`；[奇岩通路_D607](../../common/src/maprecord.inc#L3680) `(155,36) → (33,70)` |
| 绝命谷2层北部_D60022 | [绝命谷1层西部_D60012](../../common/src/maprecord.inc#L3689) `(173,60) → (46,23)`；[紫水晶洞_D604](../../common/src/maprecord.inc#L3690) `(42,19) → (31,31)`；[暗阁泉_D605](../../common/src/maprecord.inc#L3691) `(42,124) → (15,33)` |
| 绝命谷3层_D6004 | [蜈蚣洞穴_1_020](../../common/src/maprecord.inc#L3703) `(39,266) → (184,20)`；[奇岩通路_D607](../../common/src/maprecord.inc#L3705) `(39,24) → (57,69)` |
| 万年谷_D6011 | [盟重县_74](../../common/src/maprecord.inc#L3714) `(160,169) → (78,229)`；[万年原谷_D6012](../../common/src/maprecord.inc#L3715) `(29,32) → (12,16)` |
| 万年原谷_D6012 | [万年谷_D6011](../../common/src/maprecord.inc#L3724) `(11,15) → (30,33)`；[万年谷中部_D60131](../../common/src/maprecord.inc#L3725) `(202,187) → (9,51)`；[万年谷南部_D60132](../../common/src/maprecord.inc#L3726) `(9,182) → (70,66)`；[万年谷北部_D60134](../../common/src/maprecord.inc#L3727) `(283,232) → (25,17)`；[暗阁泉_D605](../../common/src/maprecord.inc#L3728) `(199,51) → (22,14)` |
| 万年谷中部_D60131 | [万年原谷_D6012](../../common/src/maprecord.inc#L3737) `(8,50) → (203,188)`；[万年独孤关_D6014](../../common/src/maprecord.inc#L3738) `(12,13) → (150,66)` |
| 万年谷南部_D60132 | [万年原谷_D6012](../../common/src/maprecord.inc#L3747) `(69,65) → (10,183)`；[万年谷西部_D60133](../../common/src/maprecord.inc#L3748) `(13,19) → (63,83)` |
| 万年谷西部_D60133 | [万年谷南部_D60132](../../common/src/maprecord.inc#L3757) `(64,82) → (14,20)`；[万年独孤关_D6014](../../common/src/maprecord.inc#L3758) `(88,75) → (14,84)` |
| 万年谷北部_D60134 | [万年原谷_D6012](../../common/src/maprecord.inc#L3767) `(26,16) → (284,233)`；[万年谷东部_D60135](../../common/src/maprecord.inc#L3768) `(86,17) → (44,43)` |
| 万年谷东部_D60135 | [万年谷北部_D60134](../../common/src/maprecord.inc#L3777) `(43,42) → (85,18)`；[万年独孤关_D6014](../../common/src/maprecord.inc#L3778) `(90,11) → (52,22)` |
| 万年独孤关_D6014 | [万年谷中部_D60131](../../common/src/maprecord.inc#L3787) `(149,65) → (13,14)`；[万年谷西部_D60133](../../common/src/maprecord.inc#L3788) `(13,83) → (87,76)`；[万年谷东部_D60135](../../common/src/maprecord.inc#L3789) `(53,21) → (89,12)`；[生死关_D6015](../../common/src/maprecord.inc#L3790) `(160,170) → (10,26)` |
| 生死关_D6015 | [万年独孤关_D6014](../../common/src/maprecord.inc#L3799) `(9,25) → (159,171)` |
| 虫峡谷1层_D6021 | [盟重县_74](../../common/src/maprecord.inc#L3808) `(41,168) → (130,505)`；[虫峡谷2层_D6022](../../common/src/maprecord.inc#L3809) `(170,23) → (40,173)` |
| 虫峡谷2层_D6022 | [虫峡谷1层_D6021](../../common/src/maprecord.inc#L3818) `(39,174) → (169,24)`；[虫峡谷3层_D6023](../../common/src/maprecord.inc#L3819) `(148,31) → (80,166)` |
| 虫峡谷3层_D6023 | [虫峡谷2层_D6022](../../common/src/maprecord.inc#L3828) `(81,167) → (149,32)`；[虫峡谷4层_D6024](../../common/src/maprecord.inc#L3829) `(161,33) → (15,191)` |
| 虫峡谷4层_D6024 | [虫峡谷3层_D6023](../../common/src/maprecord.inc#L3838) `(14,192) → (160,34)`；[虫峡谷5层_D6025](../../common/src/maprecord.inc#L3839) `(247,105) → (218,266)` |
| 虫峡谷5层_D6025 | [虫峡谷4层_D6024](../../common/src/maprecord.inc#L3848) `(219,267) → (248,106)` |
| 石灯屋_D603 | [绝命谷1层南部_D60011](../../common/src/maprecord.inc#L3856) `(16,29) → (52,15)`；[绝命谷1层西部_D60012](../../common/src/maprecord.inc#L3857) `(31,8) → (110,87)` |
| 紫水晶洞_D604 | [绝命谷1层北部_D60013](../../common/src/maprecord.inc#L3865) `(18,22) → (40,195)`；[绝命谷2层西部_D60021](../../common/src/maprecord.inc#L3866) `(17,33) → (149,133)`；[绝命谷2层北部_D60022](../../common/src/maprecord.inc#L3867) `(32,32) → (43,20)` |
| 暗阁泉_D605 | [绝命谷2层北部_D60022](../../common/src/maprecord.inc#L3875) `(14,34) → (43,125)`；[奇岩石屋_D606](../../common/src/maprecord.inc#L3877) `(22,13) → (192,123)` |
| 奇岩石屋_D606 | [绝命谷1层南部_D60011](../../common/src/maprecord.inc#L3885) `(30,30) → (150,56)`；[绝命谷1层北部_D60013](../../common/src/maprecord.inc#L3886) `(30,15) → (81,82)`；[暗阁泉_D605](../../common/src/maprecord.inc#L3887) `(193,124) → (22,14)`；[奇岩通路_D607](../../common/src/maprecord.inc#L3888) `(9,25) → (58,69)` |
| 奇岩通路_D607 | [绝命谷1层西部_D60012](../../common/src/maprecord.inc#L3896) `(32,40) → (177,34)`；[绝命谷1层北部_D60013](../../common/src/maprecord.inc#L3897) `(55,37) → (186,81)`；[绝命谷2层西部_D60021](../../common/src/maprecord.inc#L3898) `(32,71) → (155,37)`；[绝命谷3层_D6004](../../common/src/maprecord.inc#L3899) `(58,70) → (40,25)`；[奇岩石屋_D606](../../common/src/maprecord.inc#L3900) `(59,70) → (10,26)` |
| 石阁入口_D7101 | [石阁庙_75](../../common/src/maprecord.inc#L3911) `(26,23) → (207,67)`；[石阁1层_D711](../../common/src/maprecord.inc#L3912) `(25,21) → (205,198)` |
| 石阁1层_D711 | [石阁入口_D7101](../../common/src/maprecord.inc#L3925) `(205,197) → (26,20)`；[石阁2层_D712](../../common/src/maprecord.inc#L3926) `(364,229) → (364,226)` |
| 石阁2层_D712 | [石阁1层_D711](../../common/src/maprecord.inc#L3935) `(363,227) → (363,230)`；[石阁3层_D713](../../common/src/maprecord.inc#L3936) `(31,193) → (31,191)` |
| 石阁3层_D713 | [石阁2层_D712](../../common/src/maprecord.inc#L3945) `(30,192) → (30,194)`；[石阁4层_D714](../../common/src/maprecord.inc#L3946) `(350,244) → (382,190)` |
| 石阁4层_D714 | [石阁3层_D713](../../common/src/maprecord.inc#L3955) `(381,191) → (349,245)`；[石阁5层_D715](../../common/src/maprecord.inc#L3956) `(16,200) → (29,344)` |
| 石阁5层_D715 | [石阁4层_D714](../../common/src/maprecord.inc#L3965) `(28,345) → (16,201)`；[石阁6层_D716](../../common/src/maprecord.inc#L3966) `(83,81) → (36,17)` |
| 石阁6层_D716 | [石阁5层_D715](../../common/src/maprecord.inc#L3975) `(37,16) → (84,80)`；[石阁7层_D717](../../common/src/maprecord.inc#L3976) `(25,26) → (26,74)` |
| 石阁7层_D717 | [石阁6层_D716](../../common/src/maprecord.inc#L4101) `(25,75) → (24,27)` |
| 南部蚂蚁洞穴1层4_D8001 | [南部蚂蚁洞穴1层3_D8004](../../common/src/maprecord.inc#L4110) `(7,184) → (128,179)`；[未命名地图_D801](../../common/src/maprecord.inc#L4112) `(159,155) → (5,18)` |
| 南部蚂蚁洞穴1层1_D8002 | [沙漠_6](../../common/src/maprecord.inc#L4121) `(178,189) → (290,720)`；[失乐园_9](../../common/src/maprecord.inc#L4122) `(39,175) → (732,676)`；[南部蚂蚁洞穴1层2_D8003](../../common/src/maprecord.inc#L4124) `(6,43) → (185,57)` |
| 南部蚂蚁洞穴1层2_D8003 | [南部蚂蚁洞穴1层1_D8002](../../common/src/maprecord.inc#L4133) `(186,58) → (7,44)`；[南部蚂蚁洞穴1层3_D8004](../../common/src/maprecord.inc#L4134) `(29,185) → (33,21)` |
| 南部蚂蚁洞穴1层3_D8004 | [南部蚂蚁洞穴1层4_D8001](../../common/src/maprecord.inc#L4144) `(129,178) → (8,183)`；[南部蚂蚁洞穴1层2_D8003](../../common/src/maprecord.inc#L4146) `(34,20) → (30,184)` |
| 未命名地图_D801 | [南部蚂蚁洞穴1层4_D8001](../../common/src/maprecord.inc#L4156) `(4,17) → (158,154)` |
| 北部蚂蚁洞穴1层4_D8101 | [北部蚂蚁洞穴1层3_D8104](../../common/src/maprecord.inc#L4244) `(7,184) → (128,179)`；[未命名地图_D811](../../common/src/maprecord.inc#L4246) `(159,155) → (5,18)` |
| 北部蚂蚁洞穴1层1_D8102 | [绿洲_4](../../common/src/maprecord.inc#L4255) `(178,189) → (374,120)`；[北部蚂蚁洞穴1层2_D8103](../../common/src/maprecord.inc#L4256) `(6,43) → (185,57)` |
| 北部蚂蚁洞穴1层2_D8103 | [北部蚂蚁洞穴1层1_D8102](../../common/src/maprecord.inc#L4265) `(186,58) → (7,44)`；[北部蚂蚁洞穴1层3_D8104](../../common/src/maprecord.inc#L4266) `(29,185) → (33,21)` |
| 北部蚂蚁洞穴1层3_D8104 | [北部蚂蚁洞穴1层4_D8101](../../common/src/maprecord.inc#L4276) `(129,178) → (8,183)`；[北部蚂蚁洞穴1层2_D8103](../../common/src/maprecord.inc#L4278) `(34,20) → (30,184)` |
| 未命名地图_D811 | [北部蚂蚁洞穴1层4_D8101](../../common/src/maprecord.inc#L4288) `(4,17) → (158,154)` |
| 东部蚂蚁洞穴1层4_D8201 | [东部蚂蚁洞穴1层3_D8204](../../common/src/maprecord.inc#L4376) `(7,184) → (128,179)`；[未命名地图_D821](../../common/src/maprecord.inc#L4378) `(159,155) → (5,18)` |
| 东部蚂蚁洞穴1层1_D8202 | [绿洲_4](../../common/src/maprecord.inc#L4387) `(178,189) → (475,153)`；[东部蚂蚁洞穴1层2_D8203](../../common/src/maprecord.inc#L4388) `(6,43) → (185,57)` |
| 东部蚂蚁洞穴1层2_D8203 | [东部蚂蚁洞穴1层1_D8202](../../common/src/maprecord.inc#L4397) `(186,58) → (7,44)`；[东部蚂蚁洞穴1层3_D8204](../../common/src/maprecord.inc#L4398) `(29,185) → (33,21)` |
| 东部蚂蚁洞穴1层3_D8204 | [东部蚂蚁洞穴1层4_D8201](../../common/src/maprecord.inc#L4408) `(129,178) → (8,183)`；[东部蚂蚁洞穴1层2_D8203](../../common/src/maprecord.inc#L4410) `(34,20) → (30,184)` |
| 未命名地图_D821 | [东部蚂蚁洞穴1层4_D8201](../../common/src/maprecord.inc#L4420) `(4,17) → (158,154)` |
| 神舰入口_D900 | [神舰地下三层_D901](../../common/src/maprecord.inc#L4507) `(48,59) → (22,83)` |
| 神舰地下三层_D901 | [神舰入口_D900](../../common/src/maprecord.inc#L4516) `(21,85) → (47,60)`；[木工室_D9011](../../common/src/maprecord.inc#L4517) `(122,149) → (9,18)`；[木工仓库_D9012](../../common/src/maprecord.inc#L4518) `(140,129) → (9,18)`；[神舰地下二层_D9021](../../common/src/maprecord.inc#L4519) `(159,93) → (160,94)` |
| 木工仓库_D9012 | [神舰地下三层_D901](../../common/src/maprecord.inc#L4531) `(8,19) → (139,130)` |
| 神舰地下二层_D9021 | [神舰地下三层_D901](../../common/src/maprecord.inc#L4540) `(69,126) → (88,51)`；[木工室_D9011](../../common/src/maprecord.inc#L4549) `(52,110) → (18,11)`；[木工仓库_D9012](../../common/src/maprecord.inc#L4550) `(157,134) → (11,12)`；[医务室_D90211](../../common/src/maprecord.inc#L4551) `(100,52) → (28,24)`；[机械室_D90212](../../common/src/maprecord.inc#L4552) `(38,61) → (14,18)`；[神舰地下二层_D9022](../../common/src/maprecord.inc#L4553) `(57,21) → (34,69)`；[医务室_D90221](../../common/src/maprecord.inc#L4554) `(30,75) → (12,33)`；[船长室_D9033](../../common/src/maprecord.inc#L4556) `(90,145) → (19,11)` |
| 医务室_D90211 | [神舰地下二层_D9021](../../common/src/maprecord.inc#L4565) `(29,25) → (100,53)` |
| 机械室_D90212 | [神舰地下二层_D9021](../../common/src/maprecord.inc#L4573) `(15,19) → (39,62)` |
| 神舰地下二层_D9022 | [神舰地下二层_D9021](../../common/src/maprecord.inc#L4591) `(33,68) → (58,22)`；[医务室_D90221](../../common/src/maprecord.inc#L4592) `(111,146) → (28,24)`；[厨房_D90222](../../common/src/maprecord.inc#L4593) `(157,95) → (22,26)`；[士兵食堂_D90223](../../common/src/maprecord.inc#L4594) `(74,26) → (14,18)`；[神舰地下一层_D903](../../common/src/maprecord.inc#L4595) `(172,152) → (187,150)` |
| 医务室_D90221 | [神舰地下二层_D9022](../../common/src/maprecord.inc#L4603) `(29,25) → (112,147)` |
| 士兵食堂_D90223 | [神舰地下二层_D9022](../../common/src/maprecord.inc#L4615) `(15,19) → (75,27)` |
| 神舰地下一层_D903 | [神舰地下二层_D9022](../../common/src/maprecord.inc#L4627) `(105,132) → (98,151)`；[医务室_D90221](../../common/src/maprecord.inc#L4629) `(61,92) → (12,33)`；[参谋室_D9031](../../common/src/maprecord.inc#L4631) `(144,179) → (10,22)`；[长官食堂_D9032](../../common/src/maprecord.inc#L4632) `(72,60) → (7,16)`；[船长室_D9033](../../common/src/maprecord.inc#L4633) `(87,117) → (16,18)`；[调控室_D904](../../common/src/maprecord.inc#L4635) `(53,51) → (54,25)`；[慈善屋_Z013](../../common/src/maprecord.inc#L4636) `(47,41) → (10,10)` |
| 参谋室_D9031 | [神舰地下一层_D903](../../common/src/maprecord.inc#L4644) `(11,23) → (144,180)` |
| 长官食堂_D9032 | [神舰地下一层_D903](../../common/src/maprecord.inc#L4652) `(6,17) → (72,60)` |
| 船长室_D9033 | [神舰地下一层_D903](../../common/src/maprecord.inc#L4669) `(17,19) → (86,118)` |
| 调控室_D904 | [神舰地下一层_D903](../../common/src/maprecord.inc#L4678) `(54,24) → (53,52)` |
| 奸商_DM001 | [半兽洞穴2层_D002](../../common/src/maprecord.inc#L4694) `(6,10) → (191,232)` |
| 王陵_DM002 | [半兽洞穴2层_D002](../../common/src/maprecord.inc#L4701) `(7,28) → (225,176)` |
| 洞穴奸商_DM011 | [天然洞穴2层_D012](../../common/src/maprecord.inc#L4708) `(6,10) → (189,145)` |
| 半兽天然洞穴_E001 | [半兽洞穴3层_D003](../../common/src/maprecord.inc#L4717) `(8,13) → (77,230)`；[半兽天然洞穴_E002](../../common/src/maprecord.inc#L4718) `(85,15) → (85,10)` |
| 行会大战_E001 | [半兽洞穴3层_D003](../../common/src/maprecord.inc#L4727) `(8,13) → (77,230)`；[行会大战_E002](../../common/src/maprecord.inc#L4728) `(85,15) → (85,10)` |
| 半兽天然洞穴_E002 | [天然洞穴2层_D012](../../common/src/maprecord.inc#L4737) `(36,81) → (378,65)`；[半兽天然洞穴_E001](../../common/src/maprecord.inc#L4738) `(84,9) → (84,16)`；[半兽天然洞穴_E002_001](../../common/src/maprecord.inc#L4739) `(63,11) → (11,10)` |
| 行会大战_E002 | [天然洞穴2层_D012](../../common/src/maprecord.inc#L4748) `(36,81) → (378,65)`；[行会大战_E001](../../common/src/maprecord.inc#L4749) `(84,9) → (84,16)`；[半兽天然洞穴_E002_001](../../common/src/maprecord.inc#L4750) `(63,11) → (11,10)` |
| 半兽天然洞穴_E002_001 | [半兽天然洞穴_E002](../../common/src/maprecord.inc#L4757) `(12,9) → (64,13)` |
| 连接通路_E402 | [半兽洞穴3层_D003](../../common/src/maprecord.inc#L4771) `(14,72) → (291,294)`；[地下2层采矿所_D404](../../common/src/maprecord.inc#L4772) `(69,14) → (151,20)`；[连接通路_E402_001](../../common/src/maprecord.inc#L4773) `(62,71) → (25,12)` |
| 连接通路_E402_001 | [连接通路_E402](../../common/src/maprecord.inc#L4780) `(26,12) → (63,72)` |
| 竞技场_E601 | [矿山2层_D422](../../common/src/maprecord.inc#L4798) `(134,20) → (273,311)`；[竞技场_E602](../../common/src/maprecord.inc#L4799) `(13,50) → (73,66)` |
| 竞技场_E602 | [竞技场_E601](../../common/src/maprecord.inc#L4807) `(72,65) → (12,51)`；[废矿秘密通路_E603](../../common/src/maprecord.inc#L4808) `(10,9) → (25,27)` |
| 废矿秘密通路_E603 | [竞技场_E602](../../common/src/maprecord.inc#L4816) `(26,26) → (11,10)`；[废矿秘密通路_E603](../../common/src/maprecord.inc#L4817) `(107,184) → (111,181)`；[废矿秘密通路_E604](../../common/src/maprecord.inc#L4819) `(10,69) → (91,45)` |
| 废矿秘密通路_E604 | [废矿秘密通路_E603](../../common/src/maprecord.inc#L4827) `(90,44) → (9,70)`；[废矿秘密通路_E605](../../common/src/maprecord.inc#L4828) `(10,9) → (12,11)` |
| 废矿秘密通路_E605 | [废矿秘密通路_E604](../../common/src/maprecord.inc#L4837) `(13,10) → (11,10)` |
