# 5号状态、存档与结局模块联调说明

## 已实现范围

- 正式 `WorldState`：保存当前阶段、回合数、公共资源和全部剧情旗标。
- `SaveManager`：把玩家属性、技能、背包、当前位置、世界阶段、资源、旗标保存到文本文件，并支持读取恢复。
- `ProgressSystem`：统一根据 `ActionResult` 结算回合和阶段推进。
- `StatusView`：为 `status` 命令生成玩家状态、公共资源和关键旗标文本。
- `EndingSystem`：根据最终选择旗标返回结局 ID。
- `member5_tests`：覆盖世界状态、阶段推进、状态界面、存档读取、结局判定。

## 4号主循环接入方式

主循环调用任意模块后，不要在各模块里直接手动消耗回合。统一把返回值交给 `ProgressSystem`：

```cpp
ProgressSystem progress;

ActionResult result = movePlayer(ctx, "east");
std::cout << result.message << '\n';
progress.applyActionResult(result, ctx.world);
```

规则：

- `result.success == false` 时，不消耗回合，也不推进阶段。
- `result.turnConsumed == true` 时，`WorldState` 回合数加 1。
- `result.stageCompleted == true` 时，阶段推进 1 次。
- 阶段最多停在第 6 阶段，结局由最终旗标决定。

## status 命令接入方式

```cpp
std::cout << buildStatusText(ctx);
```

显示内容包括：

- 当前阶段和回合数
- 当前房间 ID
- 生命、体力、力量、智慧、声望
- 采集、攀爬、战斗、领导技能等级
- 食物、水源、士气、迁徙物资
- 已经记录在 `WorldState` 中的关键旗标

## save / load 命令接入方式

```cpp
SaveManager saveManager;

if (saveManager.saveGame("save.txt", ctx)) {
    std::cout << "保存成功。\n";
}

if (saveManager.loadGame("save.txt", ctx)) {
    std::cout << "读取成功。\n";
}
```

存档会保存：

- `WorldState` 的阶段、回合、公共资源、全部字符串旗标
- `Player` 的生命、体力、力量、智慧、声望
- 四项技能等级
- 当前房间 ID
- 背包里的物品 ID、名称、是否关键物品、数量

## 结局判定接入方式

2号事件系统负责提供结局文本，5号模块负责判断应该使用哪个结局 ID：

```cpp
EndingSystem endings;
std::string endingId = endings.determineEndingId(ctx);
std::cout << eventSystem.getEndingText(endingId);
```

当前对应关系：

| 旗标 | 结局 ID | 说明 |
|---|---|---|
| `flag_choice_resist` | `ending_resist` | 反击结局 |
| `flag_choice_hack` | `ending_hack` | 智取结局 |
| `flag_choice_migrate` | `ending_migrate` | 迁徙结局 |
| 无最终选择旗标 | `ending_fail` | 失败结局 |

## 给其他同学的注意事项

- 不要再在测试文件里临时实现 `WorldState::hasFlag()`、`WorldState::setFlag()` 等函数，否则会和正式 `WorldState.cpp` 冲突。
- 1号地图、2号事件、3号战斗、4号玩家动作都只需要返回 `ActionResult`，最终回合和阶段结算交给 `ProgressSystem`。
- 读取存档后，只要 `WorldState` 里保留了 `flag_pending_<eventId>`，2号事件系统就可以恢复待选择事件。
