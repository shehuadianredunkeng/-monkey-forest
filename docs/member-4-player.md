# 4号：拾取与背包行为说明

只处理拾取、消耗和背包文字输出。不涉及智慧成长、技能、界面、NPC、战斗、主线、存档。

## 文件

| 文件 | 说明 |
|---|---|
| `src/PlayerActions.cpp` | 拾取、使用物品、背包文字输出 |
| `src/Inventory.cpp` | 逐件递减、归零删槽、重要道具拒删 |
| `src/main.cpp` | `takeMapItem` 写拾取标记、按互动类型分派 |
| `tests/member4_player_test.cpp` | 回归用例 11 组 |

## 行为

拾取不是打字命令，而是**和场景物件互动**：地图给出目标，主循环按 `InteractionKind`
分派，物品类走 `takeMapItem(interaction.id, ctx)`。

- `takeItem(itemId, ctx)` 按正式 ID、英文短名或中文名拾取**一件**。
- 房间不存在、物品认不出、房间没有、背包已满 → 失败，不消耗回合。
- 成功 → `success=true, turnConsumed=true`。
- 物品动作不写 `WorldState`，回合由 `ProgressSystem` 统一结算。
- `useItem` 只消耗草药、果实、蜂蜜。藤索、燧石、晶片、四季信物是剧情道具，不会被消耗；
  `Inventory::removeItem` 对重要道具直接拒绝，是第二道防线。
- 拾取标记 `flag_taken_<roomId>_<物品ID>` 由 `takeMapItem` 写，读档后不重复出现。
  `Room::getItemIds()` 不感知拾取状态。

背包格式（上限 `Inventory::MAX_SLOTS` = 12）：

```text
背包 3/12
- 果实 x3 [fruit / 果实]
- 草药 x2 [herb / 草药]
- 星猿晶片 x1 [chip / 晶片]
```

空背包显示 `背包 0/12` 和 `背包为空。`。名称取自 `findItemInfo` 表，存档里存旧名也按
正式中文名显示，不暴露 `item_` 这类内部 ID。

## 验证

`cmake -S . -B build -G "Visual Studio 17 2022" -A x64` 构建，
`ctest --test-dir build -C Release --output-on-failure` 跑测试。
11 组用例覆盖物品/背包、玩家属性、消耗递减、剧情道具不消耗、拾取别名、背包显示。

## 已知限制

- `lookAround` 仍直接显示 `getItemIds()`，拾取后的场景移除要地图/状态模块用公共接口做。
- `tests/run_member4_checks.sh` 的 `main` 套件引用已删除的 `member4_main_test.cpp`，
  当前跑不起来；其余套件正常。
