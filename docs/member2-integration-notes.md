# 2号剧情事件模块最终联调说明

## 1. 本轮完成内容

- 六阶段中文主线、8个主线事件、3个随机事件与4种结局文本。
- 玩家界面只显示中文事件名、阶段名、路线名、地点名和任务目标。
- 所有事件选项直接标明属性、技能、物品、资源和路线准备条件。
- 进入正确房间后可由主循环调用2号接口自动展示主线事件。
- `investigate` 保留为重新查看当前事件的兼容入口。
- 当前地点存在随机事件时，中文主线目标会提示“随机（random）”入口。
- 主动探索可以完成全部随机事件；每个事件仍只能完成一次。
- 每次选择后显示结果和下一步，不向玩家显示内部ID或程序调用名。
- 野蜂、巡逻机、赫兹战斗胜利后可由主循环调用2号接口自动续接剧情。
- 5个一次性智慧成长节点可随存档保存，错过早期节点仍可达到智慧4。
- 反击、智取、迁徙结局同时检查路线准备和最终数值，不能只堆数值跳结局。
- 读档后优先恢复存档中的待选择事件，不受读档前内存缓存干扰。

## 2. 与测试问题表的对应

| 2号问题 | 代码处理 |
|---|---|
| 主线显示全面中文化 | `getCurrentObjective()`、事件提示、阶段和结局均使用中文名，内部ID不进入玩家提示 |
| 事件到达后自动触发 | 新增 `triggerAvailableMainEvent(ctx)`，4号在移动成功后调用 |
| 随机事件有入口和提示 | 当前地点可触发时由 `getCurrentObjective()`提示“随机（random）”；取消最多2次限制 |
| 选项条件透明 | 11个事件的每个选项都直接写明条件或“无属性要求” |
| 选项流程文字清晰 | 结果说明具体变化；完成后提示“指引（guide）”；战斗后提供自动续接接口 |
| 丰富主线和结局 | 六阶段补足因果关系，三条路线记录不同准备状态，四个结局扩写前因与结果 |

## 3. 与4号主循环的对接

### 3.1 初始化

```cpp
EventSystem events;
events.initializeEvents();
```

### 3.2 中文主线目标

4号的 `guide` / `指引` 命令直接显示：

```cpp
std::cout << events.getCurrentObjective(ctx) << "\n";
```

不要在 `main.cpp` 另写一套事件ID、目标房间或英文任务名。

### 3.3 进入房间后自动触发

移动成功并显示房间内容后调用：

```cpp
ActionResult autoEvent = events.triggerAvailableMainEvent(ctx);
if (!autoEvent.message.empty()) {
    applyAndPrint(autoEvent, ctx, progress, events);
}
```

该接口会检查阶段、房间、前置状态、已完成状态和待选择状态；不会重复发奖，
也不会因为玩家反复进出房间重复创建事件。事件提示本身不消耗回合。

### 3.4 玩家主动调查和选择

```cpp
ActionResult prompt = events.triggerAvailableMainEvent(ctx);
ActionResult choice = events.chooseEventOption("", option, ctx);
```

`investigate` 可调用 `triggerAvailableMainEvent(ctx)` 重新显示；`choose` / `选择`
只需要传入选项编号，不要求玩家输入事件ID。

### 3.5 随机事件

玩家输入 `random` / `随机` 时调用：

```cpp
ActionResult randomEvent = events.triggerEvent("random", ctx);
```

随机事件不再限制每局最多2个。山火、隐藏果园、侦察机坠落均可各完成一次，
完成状态分别保存在自己的事件完成旗标中。3号成就系统可以直接读取这些完成状态。

### 3.6 战斗结束后自动续接事件

3号战斗系统结束并写入胜利旗标后，4号调用：

```cpp
ActionResult continued = events.resumePendingEventAfterBattle(ctx);
if (!continued.message.empty()) {
    applyAndPrint(continued, ctx, progress, events);
}
```

该接口只在对应敌人已经胜利时继续事件，并固定续接原事件第一项；重复调用不会
重复奖励或重复推进阶段。主循环不再要求玩家战后再次输入 `choose 1`。

### 3.7 回合与阶段

2号事件系统不直接调用 `consumeTurn()`。4号只在
`ActionResult.turnConsumed == true` 时扣除回合；当
`stageCompleted == true` 时继续交给5号阶段系统推进。自动展示事件和等待战斗
不会额外扣回合。

## 4. 与3号NPC和战斗的对接

### 4.1 战斗旗标保持不变

| 2号产生的待战斗状态 | 3号敌人 | 3号胜利后产生 |
|---|---|---|
| `flag_pending_battle_bees` | `enemy_bees` | `flag_bees_defeated` |
| `flag_pending_battle_robot` | `enemy_robot` | `flag_robot_defeated` |
| `flag_pending_battle_hertz` | `enemy_hertz` | `flag_hertz_defeated` |

3号可以继续实现攻击、防御、偷窃、逃跑、同伴追击和特殊战斗选项；2号只负责
战斗前后的剧情状态，不读取3号战斗内部回合数据。

### 4.2 豆豆流程归3号

按3号最新修改说明，2号已删除“幼猴受伤”随机事件及其状态处理。豆豆受伤、
草药判断、救治选项、神秘祝福和后续对话全部由3号NPC连续 `talk` 流程负责，
不会再出现事件系统和NPC系统各自维护一套豆豆状态的问题。

2号文案不使用 `quest`、`finish` 或 `npc_*` 玩家指令，也不会覆盖3号设置的
闪尾协助、豆豆祝福和成就状态。基地潜入仍读取公共的 `flag_scout_help`，供3号
完成闪尾协助后写入。

## 5. 与4号玩家属性和物品的对接

2号继续只使用公开接口：

- `Player::getWisdom()` / `Player::changeWisdom()`；
- `Player::getSkillLevel()`；
- `Player::hasItem()` / `Player::addItem()`；
- `WorldState` 公共资源和字符串旗标接口。

没有新增 Wisdom 技能，也没有直接访问玩家私有属性。星猿晶片研究只增加理解，
不消耗 `item_chip`。获得晶片时会同时加入真实背包物品和剧情状态；背包已满时
事件暂不结算，避免只有旗标没有物品。

## 6. 五个智慧成长节点

| 节点 | 事件选项 | 奖励与持久状态 |
|---|---|---|
| 树冠观察 | 树冠试炼第三项 | 智慧+1；事件完成状态防重复 |
| 管线规律 | 发光河水第二项 | 无智慧前置，智慧+1；事件完成状态防重复 |
| 回声碑文 | 回声追踪第一项 | 无智慧前置，智慧+1；事件完成状态防重复 |
| 星猿技术 | 侦察机第一项或晶片研究 | 两个入口共享一次智慧+1，互查已有路线/分析状态 |
| 基地日志 | 基地潜入第二项 | 智慧达到3后+1，可补到最终要求4 |

玩家初始智慧为1，完成任意3个可用节点即可达到智慧4。所有判定依靠可保存的
世界状态，不使用只存在于 `EventSystem` 内存中的临时奖励变量。

## 7. 三条结局路线

| 路线 | 必须完成的剧情准备 | 最终条件 |
|---|---|---|
| 反击 | 守卫训练或说服族群反击 | 反击准备、声望≥60、战斗≥2，随后击败赫兹 |
| 智取 | 研究星猿晶片并取得完整日志 | 技术研究准备、智慧≥4、完整日志 |
| 迁徙 | 找到新家园并完成两次物资准备 | 迁徙准备、物资≥8、领导≥2、新家园 |

最终事件会同时检查路线准备旗标和数值条件，确保结局由此前剧情选择产生，
而不是只在最后临时堆高数值。

## 8. 5号存档需要保留的2号状态

- 全部 `flag_event_*_done` 事件完成状态；
- 全部 `flag_pending_event_*` 待选择状态；
- `flag_route_resist_ready`、`flag_route_hack_ready`、`flag_route_migrate_ready`；
- `flag_water_fixed`、`flag_chip_found`、`flag_base_open`、`flag_complete_log`；
- `flag_new_home_found`、`flag_drone_analyzed`、`flag_scout_help`；
- 三个待战斗状态与三个敌人胜利状态；
- `flag_choice_resist`、`flag_choice_hack`、`flag_choice_migrate`、`flag_final_choice`。

读档后 `chooseEventOption("", option, ctx)` 会优先读取存档中的待选择事件，
不会错误沿用读档前的活动事件缓存。

## 9. 分工边界

本交付只修改2号文件：`EventSystem.h`、`EventFactory.cpp`、`EventSystem.cpp`、
`StoryText.cpp`、2号测试和本说明。未修改地图、NPC、战斗、玩家、背包、主循环、
存档、状态显示或结局判定文件。

## 10. 验收测试

2号专项测试覆盖：

- 8个主线与3个随机事件均可触发；
- 所有选项条件透明，玩家提示不显示内部ID；
- 进入正确房间自动展示主线且防重复；
- 当前地点存在随机事件时显示中文入口；
- 三个随机事件均能在同一局完成；
- 战斗胜利后自动续接原剧情且不重复结算；
- 错过树冠奖励后仍能达到智慧4；
- 晶片与侦察机研究不重复增加智慧；
- 反击、智取、迁徙三条路线均可达；
- 未完成路线准备时，单靠最终数值不能跳转结局；
- 读档待选事件覆盖旧内存缓存；
- 六阶段和四个结局文本均为中文。

同时已使用现有统一工程运行1号、2号、3号、4号、5号测试，全部通过。
