# 4号模块联调说明

## 已实现范围

- `Item`：正式物品 ID 对应的基础数据、关键物品标记和非负数量管理。
- `Inventory`：8 个不同物品槽位、同 ID 堆叠、单件删除和关键物品保护。
- `Player`：属性边界、四项技能、背包代理接口和当前位置。
- `PlayerActions`：`takeItem`、`useItem`、`trainSkill`、`rest`、`showInventory`。

## 公共接口处理

- `Player` 和 `Inventory` 严格使用 Final 1.0 的公开接口。
- 仓库当前把 `GameContext` 定义在 `CommonTypes.h`，因此本模块直接使用该定义，未创建第二套 `GameContext`。
- 未创建 `SkillSystem`、`GameLoop`、`WorldState` 或 `main`。
- `tests/TestSupport.cpp` 仅把占位实现 `Player::getCurrentRoomId()` 的返回类型同步为 Final 1.0 的 `const std::string&`，用于保持原地图测试可编译。

## 关键物品规则

`item_rope`、`item_flint` 和 `item_chip` 在 `PlayerActions` 创建时标记为关键物品。`Inventory::removeItem` 拒绝删除标记为关键的物品。普通物品每次删除一个数量单位。

## 需要后续成员联调

1. `Room` 目前只公开只读的 `getItemIds()`，没有物品移除接口。因此 `takeItem` 成功后不能安全地从房间永久移除该物品；地图或 `WorldState` 模块需要统一决定房间物品的持久状态。
2. `item_rope`、`item_flint`、`item_chip` 的剧情效果属于地图/事件系统；`useItem` 不修改世界状态，也不推进剧情。
3. 所有玩家动作只设置 `ActionResult.turnConsumed`，不会直接消耗回合；最终主循环负责统一处理回合。
4. `tests/member4_player_test.cpp` 为链接现有 `Room.cpp` 提供了测试专用的 `WorldState::hasFlag` 最小实现。整合正式 `WorldState.cpp` 后，应避免在同一测试目标中同时链接两份实现。

## 构建与测试

仓库 CMake 已增加 `monkey_player` 和 `member4_tests` 目标。标准验证命令：

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

4号开发环境未提供 CMake，因此提交前使用 `g++ 13.3.0 -std=c++17 -Wall -Wextra -Wpedantic` 分别编译并运行原地图测试和 `member4_player_test`。
