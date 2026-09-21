# 4号：拾取与背包行为说明

## 范围

只处理拾取和背包这几件事：无参数批量拾取、单件消耗语义、背包文字输出。没有涉及
智慧成长、技能规则、颜色/双栏/进度条、NPC、战斗、主线、存档或结局。公开头文件和
函数签名保持不变。

## 涉及的文件

| 文件 | 变化 |
|---|---|
| `src/PlayerActions.cpp` | 批量拾取、既有拾取旗标只读检查、统一名称/别名、背包格数、消耗成功后才生效 |
| `src/main.cpp` | 只调整 `takeItemOnce` 和 `take` 分派两处 |
| `tests/member4_player_test.cpp` | 玩家/物品回归用例 16 组 |
| `tests/member4_main_test.cpp` | 真实整合入口测试 7 组，含实际输入循环 |
| `tests/run_member4_checks.sh` | 无第三方框架的构建与测试脚本 |

`Inventory::removeItem` 已经是逐件递减、归零删槽，因此没有重写 `Inventory.cpp`。
新测试覆盖草药 `x3 -> x2 -> x1 -> 删除`，以及删除不存在的物品。
另外修了 `useItem` 忽略删除失败的问题：物品受保护时，消耗失败就不再恢复生命/体力。

## 行为

- `takeItem("", ctx)` 按 `Room::getItemIds()` 的顺序尝试所有尚未拾取的物品。
- 背包空间不足时保留已经拿到的物品，并在同一条消息里列出没拿到的；仍继续尝试后面
  已经存在的堆叠。
- 至少拿到一件：`success=true, turnConsumed=true, stageCompleted=false`；
  一件都没拿到：不消耗回合。
- `takeItem("item_herb", ctx)` 仍只取草药。正式 ID、英文短名和中文物品名都兼容。
- 所有物品动作都不调用 `consumeTurn()`、不写 `WorldState`。回合仍由 `ProgressSystem`
  统一结算一次。
- 主循环接受 `take` 或 `take <一个目标>`，不解析 `;`、`&&`、逗号命令链；多目标的
  `take` 会被拒绝。
- `chip/rope/flint` 不作为普通消耗品删除。没有新增中文动作命令。

背包输出格式：

```text
背包 3/8
- 果实 x3 [fruit / 果实]
- 草药 x2 [herb / 草药]
- 星猿晶片 x1 [chip / 晶片]
```

其余映射：`藤索 [rope / 藤索]`、`燧石 [flint / 燧石]`。
空背包显示 `背包 0/8` 和 `背包为空。`。五种正式物品即使存档里存的是旧名称，显示时
也统一用中文名，不暴露内部 ID。

## 两个必须一起接入的文件

`src/PlayerActions.cpp` 和 `src/main.cpp` 要成对接入，两处改动互相配合：玩家动作读取
既有的 `flag_taken_<roomId>_<正式物品ID>`，主循环按背包数量变化只标记本次实际获得的
物品。只更新其中之一会出问题。

无参数拾取不写空目标旗标；失败或没装下的物品不标记，腾出空间后可以重试。
不同别名共用同一个正式 ID 旗标，换别名不能重复拾取。

## 验证

构建使用 C++17，开启 `-Wall -Wextra -Wpedantic -Werror`。在仓库根目录：

```bash
# 只验证地图和玩家测试
bash tests/run_member4_checks.sh ../member4-checks

# 完整工程作为依赖，自动链接本轮的 main 和 PlayerActions
bash tests/run_member4_checks.sh ../member4-checks ../member5-checkout/monkey_forest

# 同一组检查开启 UBSan
MEMBER4_SANITIZE=1 bash tests/run_member4_checks.sh ../member4-ubsan ../member5-checkout/monkey_forest
```

结果：玩家单元测试 16/16 通过，主循环接入测试 7/7 通过；完整工程的地图、2号、3号、
4号、5号测试均通过。入口测试执行真实输入循环，检查 `take`、别名、重复拾取、部分重试
和回合计数。同一组检查在 UBSan
（`-fsanitize=undefined -fno-sanitize-recover=all`）下再次全部通过。

完整工程的标准流程：

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

若用本目录的 `member4_player_test.cpp` 替换整合版同名测试，并链接正式
`WorldState.cpp`，需要为该测试目标定义 `MEMBER4_USE_REAL_WORLD`，关掉仅供独立分支
使用的测试桩：

```cmake
target_compile_definitions(member4_tests PRIVATE MEMBER4_USE_REAL_WORLD)
```

验证脚本已经分别处理独立测试桩和正式 WorldState，两者不会在同一测试中重复定义。

## 已知限制

- 房间物品的「不可再次获得」复用了主循环既有的拾取旗标，但 `lookAround` 仍直接显示
  原始 `getItemIds()`。物品拾取后的场景移除需要地图/状态模块用正式公共状态接口完成。
- 无参数拾取的教学文案和帮助界面更新留给界面负责人。
