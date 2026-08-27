# 2号剧情事件模块联调说明

## 已实现内容

- 六个阶段的开场文字。
- 8个主线事件与4个随机事件。
- 事件的阶段、房间、前置旗标和防重复触发检查。
- 事件选项对玩家属性、公共资源、物品和剧情旗标的影响。
- 反击、智取、迁徙、失败四种结局文本。
- 野蜂、巡逻机、赫兹三场战斗桥接。
- 随机事件每局最多完成两个。
- 读取存档后通过 `flag_pending_<eventId>` 恢复待选择事件。

## 与1号地图的对接

2号事件使用1号提供的五个统一房间ID：

- `room_tree`
- `room_forest`
- `room_river`
- `room_cave`
- `room_base`

1号不需要修改事件或战斗代码。

## 与4号主循环的对接

游戏初始化时调用：

```cpp
EventSystem eventSystem;
eventSystem.initializeEvents();
```

触发指定事件：

```cpp
ActionResult result = eventSystem.triggerEvent(eventId, ctx);
```

玩家输入 `choose 1` 时调用：

```cpp
ActionResult result = eventSystem.chooseEventOption("", 1, ctx);
```

事件系统不会直接调用 `WorldState::consumeTurn()`。主循环只在
`result.turnConsumed == true` 时统一消耗一回合；当
`result.stageCompleted == true` 时，由4号主循环通知5号阶段系统推进阶段。

当事件系统设置待战斗旗标时，4号主循环按下表调用3号战斗系统：

| 待战斗旗标 | 4号主循环调用 |
|---|---|
| `flag_pending_battle_bees` | `startBattle("enemy_bees", ctx)` |
| `flag_pending_battle_robot` | `startBattle("enemy_robot", ctx)` |
| `flag_pending_battle_hertz` | `startBattle("enemy_hertz", ctx)` |

这些函数名只写在联调说明中，不会作为剧情文字显示给玩家。

## 与3号NPC和战斗的对接

| 待战斗旗标 | 敌人ID | 3号胜利后产生 |
|---|---|---|
| `flag_pending_battle_bees` | `enemy_bees` | `flag_bees_defeated` |
| `flag_pending_battle_robot` | `enemy_robot` | `flag_robot_defeated` |
| `flag_pending_battle_hertz` | `enemy_hertz` | `flag_hertz_defeated` |

战斗胜利后，玩家再次执行原事件选项，事件系统会读取胜利旗标并完成剧情。
3号现有敌人ID与胜利旗标已经匹配，不需要修改战斗接口。

幼猴随机事件现在产生 `flag_child_found`，随后3号的
`completeNPCQuest("npc_child", ctx)` 负责产生 `flag_child_rescued`，两模块不会再互相越权。

## 与4号玩家和物品的对接

事件获得晶片时会同时：

- 把真实的 `Item("item_chip", "芯片", true, 1)` 放入背包；
- 设置 `flag_chip_found`。

如果背包已满，事件暂不结算，玩家整理背包后可重新选择，避免只获得旗标却没有晶片物品。

## 交给5号保存的旗标

5号应保存全部字符串旗标，尤其包括：

- 主线完成：`flag_event_<事件名>_done`
- 待选择事件：`flag_pending_<eventId>`
- 关键进度：`flag_water_fixed`、`flag_chip_found`、`flag_base_open`、`flag_complete_log`
- 路线准备：`flag_route_resist_ready`、`flag_route_hack_ready`、`flag_route_migrate_ready`
- 最终选择：`flag_choice_resist`、`flag_choice_hack`、`flag_choice_migrate`
- 战斗桥接：三个 `flag_pending_battle_*` 与三个敌人胜利旗标

迁徙路线的两次准备各提供4份物资，正常主线可以达到最终要求的8份物资。

## 结局ID

| 结局ID | 文本 |
|---|---|
| `ending_resist` | 青木英雄 |
| `ending_hack` | 无声胜利 |
| `ending_migrate` | 向南的新生 |
| `ending_fail` | 失落之谷 |

5号负责判断结局ID，2号的 `getEndingText(endingId)` 只负责返回对应剧情。

## 后续成员顺序

1. 2号先上传本事件模块。
2. 4号拉取2号代码，完成主循环与三场战斗的连接。
3. 5号补齐正式 `WorldState.cpp`、阶段推进、结局判定，并进行最终测试和联调整合。

截至本分支定稿时，仓库还没有5号的正式 `WorldState.cpp`，也没有最终主循环和 `main()`。
因此现阶段以独立模块测试验证联调；4号和5号完成后，再构建完整游戏程序。

## 测试

`tests/member2_event_test.cpp` 已覆盖：

- 全部12个事件可触发；
- 防重复与随机事件上限；
- 存档待选事件恢复；
- 真实晶片物品；
- 幼猴事件与NPC任务；
- 野蜂真实战斗；
- 迁徙结局可达；
- 六阶段和四结局文本入口。
