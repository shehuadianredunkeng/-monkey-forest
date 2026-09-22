# 4号：拾取与背包行为说明

## 范围

只处理拾取和背包这几件事：单件拾取、消耗语义、背包文字输出。没有涉及
智慧成长、技能规则、颜色/双栏/进度条、NPC、战斗、主线、存档或结局。

## 涉及的文件

| 文件 | 说明 |
|---|---|
| `src/PlayerActions.cpp` | 拾取、使用物品、背包文字输出 |
| `src/Inventory.cpp` | 逐件递减、归零删槽；重要道具拒绝删除 |
| `src/main.cpp` | `takeMapItem` 记录已拾取标记，并按互动类型分派 |
| `tests/member4_player_test.cpp` | 玩家/物品回归用例 11 组 |

## 拾取方式

拾取不是打字命令，而是**和场景物件互动**：地图给出互动目标，主循环按
`InteractionKind` 分派，物品类走 `takeMapItem(interaction.id, ctx)`。

- `takeItem(itemId, ctx)` 按正式 ID、英文短名或中文名拾取**一件**物品。
- 房间不存在、物品认不出来、房间没有该物品、背包已满，都返回失败且不消耗回合。
- 成功拾取返回 `success=true, turnConsumed=true`。
- 所有物品动作都不调用 `consumeTurn()`、不写 `WorldState`。回合仍由
  `ProgressSystem` 统一结算一次。

## 消耗语义

`useItem` 只消耗草药、果实、蜂蜜三样。藤索、燧石、星猿晶片和四季信物属于剧情道具，
使用时会走到拒绝分支，不会被消耗。

`Inventory::removeItem` 对标记为重要的道具直接返回失败，这是第二道防线：
即使哪天有代码走了消耗分支，重要道具也删不掉。

## 背包输出格式

```text
背包 3/12
- 果实 x3 [fruit / 果实]
- 草药 x2 [herb / 草药]
- 星猿晶片 x1 [chip / 晶片]
```

容量上限是 `Inventory::MAX_SLOTS`（当前 12）。空背包显示 `背包 0/12` 和 `背包为空。`。

显示名称来自 `findItemInfo` 的物品表，不把 `item_fruit` 这类内部 ID 给玩家看；
存档里存的是旧名称也一样按正式中文名显示。其余映射：`藤索 [rope / 藤索]`、
`燧石 [flint / 燧石]`。

## 拾取标记

`takeMapItem` 负责写 `flag_taken_<roomId>_<物品ID>`，读档后物品不会重新出现。
`Room::getItemIds()` 不感知拾取状态，仍返回房间的原始物品列表。

## 验证

构建使用 C++17，标准流程：

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

`tests/member4_player_test.cpp` 共 11 组用例，覆盖：物品与背包基础、玩家属性、
消耗品逐件递减、剧情道具不被消耗、蜂蜜与四季信物的识别、战斗掉落物识别、
指名拾取不会连带拿走别的物品、背包显示用正式名、空背包显示。

## 已知限制

- `lookAround` 仍直接显示原始 `getItemIds()`，物品拾取后的场景移除需要地图/状态模块
  用正式公共状态接口完成。
- `tests/run_member4_checks.sh` 里的 `main` 套件引用 `tests/member4_main_test.cpp`，
  该文件已随整合版删除，因此该套件当前跑不起来；脚本其余套件（map/member2/3/4/5）
  不受影响。
