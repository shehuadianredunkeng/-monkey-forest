# 3号模块联调说明

## 已实现范围

- 五名NPC的分阶段对话、任务反馈和最终协助判定。
- 野蜂群、巡逻机、赫兹三类敌人与轻量回合制战斗。
- `attack`、`guard`、`use`、`escape`基础行动，以及`analyze`、`hack`扩展行动。
- 闪尾任务包含两条对话路线，交付藤蔓后解锁“逃跑（escape）”。
- 若玩家向闪尾承诺香蕉，赫兹战会出现香蕉诱惑、智慧判定与两个坏结局。
- 基础战斗动作精简为攻击、防御、偷窃、背包和逃跑；分析、破解等只在特殊战斗提示。
- 每场战斗限偷窃一次：蜂群掉落蜂蜜、巡逻机掉落材料碎片、赫兹掉落研究手册。
- 蜂群连续防御采用递减收益，第三次不再反伤并触发成就标记。
- 救回豆豆后，战斗回合可能触发回血、三次攻击翻倍或直接击败敌人的祝福。
- 燧石可在战斗中发动20点火攻，但可能触发“放火烧山”坏结局。
- 闪尾减伤、叶婆婆战中恢复、猴王增伤三种NPC协助效果。
- 赫兹能源护甲、巡逻机每三回合蓄力、野蜂防御反制等差异机制。

## 与1号、4号的对接

- NPC位置已写入五张地图：岩背/叶婆婆在猴王树，闪尾在果实森林，豆豆在河谷，赫兹在基地。
- 战斗中的`use`直接调用4号`PlayerActions.h`提供的`useItem`。
- 藤索被4号定义为关键物品，完成闪尾任务时仅检查并保留，不调用`removeItem`。
- 新增`GameContext.h`兼容入口；实际类型仍沿用1号、4号`CommonTypes.h`中的定义。
- 4号主循环在解析`talk 闪尾 1`或`对话 闪尾 1`后，调用`chooseNPCDialogue("闪尾", 1, ctx)`。
- 场景按“中文名（英文名）”显示NPC；玩家可输入任一名称，如`talk 闪尾`或`talk scout`。
- `npc_scout`等带`npc_`前缀的编号仅供程序内部兼容，不向玩家展示。
- 玩家侧不再提供`quest`和`finish`：所有NPC任务均由`talkToNPC`获取，满足条件后再次调用同一接口自动提交。
- `getNPCQuest`和`completeNPCQuest`仅作为旧代码兼容接口保留，不应再注册为玩家命令。
- 战斗中的香蕉选择沿用现有动作接口：`performBattleAction("香蕉", "1", ctx)`。

## 交给2号和5号的旗标

- 2号产生：`flag_water_fixed`、`flag_child_found`、`flag_complete_log`。
- 3号产生：`flag_scout_help`、`flag_skill_escape_unlocked`、`flag_scout_banana_promise`、
  `flag_bad_ending_second_banana`、`flag_bad_ending_gluttony`、`flag_healer_supplied`、
  `flag_child_rescued`、`flag_king_support`、三种敌人击败旗标，以及以下成就标记：
  `flag_achievement_monkey_borrow`、`flag_achievement_you_fight_back`、
  `flag_achievement_doudou_bond`、`flag_achievement_no_rice`、
  `flag_achievement_next_line_after_forest_fire`（“放火烧山的下一句”）。
- 5号负责把这些成就标记永久保存到独立成就文件；不要与普通存档一起覆盖。
- `random`次数限制属于2号`EventSystem`，3号分支不直接修改该模块。
- 5号需要按纯接口实现完整`WorldState`；3号测试中的状态实现仅用于独立测试。

## 战斗命令

玩家界面统一显示中文命令；为兼容4号主循环，系统同时接受括号中的英文内部命令。

```text
攻击（attack）
防御（guard）
偷窃（steal，每场限一次）
背包（inventory）
分析（analyze）
破解（hack）
使用 item_herb（use item_herb）
使用 item_fruit（use item_fruit）
使用 燧石（use flint）
逃跑（escape）
香蕉 1/2/3（banana 1/2/3，仅赫兹诱惑阶段）
```
