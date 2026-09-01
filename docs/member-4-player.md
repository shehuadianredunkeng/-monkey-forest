# 4号：2026-09-01 拾取与背包回归修复

## 本轮范围

只处理已确认的1～4：无参数批量拾取、不新增一行多命令、单件消耗语义及背包文字输出。
没有修改智慧成长、技能规则、颜色/双栏/进度条、NPC、战斗、主线、存档或结局。
公开头文件和函数签名保持不变。

## 基线和文件

- 开发/提交分支：`member-4-player`，基线 `162b3091e65e0e92fa32fec127140ec38f1a7edf`。
- 5号完整整合参考：`member-5-zip` 的 `9afe9d302318977bd4e8273bac4b2600c17496f9`，工程在 `monkey_forest/` 子目录。
- 仓库当前没有 `main` 分支，不创建指向其他成员分支的 PR，也不自动合并。

| 文件 | 本轮变化 |
|---|---|
| `src/PlayerActions.cpp` | 批量拾取、既有拾取旗标只读检查、统一名称/别名、背包格数、消耗成功后才生效 |
| `src/main.cpp` | 基于获授权的5号入口；仅调整 `takeItemOnce` 和 `take` 分派两处 |
| `tests/member4_player_test.cpp` | 16组玩家/物品回归用例，含原有测试 |
| `tests/member4_main_test.cpp` | 7组真实整合入口测试，包括实际输入循环 |
| `tests/run_member4_checks.sh` | 无第三方测试框架的可复跑构建与测试脚本 |
| 本文 | 范围、验证和交接说明 |

`Inventory::removeItem` 在当前分支已经逐件递减、归零删槽，因此未重写 `Inventory.cpp`。
新测试覆盖草药 `x3 -> x2 -> x1 -> 删除` 和删除不存在物品。
另修复了 `useItem` 忽略删除失败的问题：如果物品受到保护，消耗失败就不会恢复生命/体力。

## 行为

- `takeItem("", ctx)` 按 `Room::getItemIds()` 顺序尝试所有尚未拾取的物品。
- 背包空间不足时保留已经获得的物品，并在同一消息中列出未获得的物品；仍继续尝试后面的已有堆叠。
- 至少获得一件：`success=true, turnConsumed=true, stageCompleted=false`；一件也没获得：不消耗回合。
- `takeItem("item_herb", ctx)` 仍只取草药；正式 ID、英文短名和中文物品名均兼容。
- 所有物品动作依然不调用 `consumeTurn()`、不写 `WorldState`。整体回合仍由原 `ProgressSystem` 结算一次。
- 主循环允许 `take` 或 `take <一个目标>`，不会解析 `;`、`&&`、逗号命令链；多目标/多词的 `take` 被拒绝。
- `chip/rope/flint` 不作为普通消耗品删除。没有新增中文动作命令，保留原主循环已有功能。

```text
背包 3/8
- 果实 x3 [fruit / 果实]
- 草药 x2 [herb / 草药]
- 星猿晶片 x1 [chip / 晶片]
```

其余映射：`藤索 [rope / 藤索]`、`燧石 [flint / 燧石]`。
空背包为 `背包 0/8` 和 `背包为空。`。五种正式物品即使保存着旧名称，显示时也使用统一中文名，不暴露内部 ID。

## 必须一起接入的两个文件

4号分支原来只有模块和测试，没有整合入口或其他成员的完整依赖。
本次 `src/main.cpp` 是5号 `monkey_forest/src/main.cpp` 的授权修改版，保留其余游戏逻辑。
没有把整个5号分支或其他模块复制/合并进4号分支，也没有推送5号分支。

整合者应把本分支以下两个文件一起接入完整工程对应的 `src/`：

1. `src/PlayerActions.cpp`
2. `src/main.cpp`

两处改动互相配合：玩家动作读取既有 `flag_taken_<roomId>_<正式物品ID>`，主循环按背包数量变化只标记本次实际获得的物品。
无参数拾取不会写入空目标旗标；失败/未装下的物品不标记，可以腾出空间后重试。
不同别名共用同一个正式 ID 旗标，不会通过更换别名重复拾取。
不要只更新主循环或只更新玩家动作。

## 实际验证

本次环境有 `g++ 13.3.0`，没有 CMake；因此不声称已经运行 CMake/CTest。
所有实际编译均使用 C++17，开启 `-Wall -Wextra -Wpedantic -Werror`。

在4号仓库根目录，使用 Bash 和 g++：

```bash
# 仅验证4号分支的地图和玩家测试。
bash tests/run_member4_checks.sh ../member4-checks

# 完整5号工程只读作为依赖，自动链接本轮修复版的 main 和 PlayerActions。
# 不需要复制或修改5号测试目录，输出写入第一个参数指定的目录。
bash tests/run_member4_checks.sh ../member4-checks ../member5-checkout/monkey_forest

# 同一组检查开启 UBSan。
MEMBER4_SANITIZE=1 bash tests/run_member4_checks.sh ../member4-ubsan ../member5-checkout/monkey_forest
```

实际检查结果：

- 修改前：原4号分支测试通过；完整5号项目原有地图、2/3/4/5号五组测试均通过。
- 红灯回归：玩家测试7通过/9失败，入口测试1通过/6失败，失败点对应本轮缺失行为。
- 修改后：4号单元测试16/16、主循环接入测试7/7通过；完整工程地图、2号、3号、4号、5号测试通过。
- 实际构建了完整游戏可执行文件；入口测试执行真实输入循环，检查 `take`、别名、重复拾取、部分重试和回合计数。
- 同一组完整检查在 UBSan（`-fsanitize=undefined -fno-sanitize-recover=all`）下再次全部通过。
- 只读代码审查未发现阻塞性问题；`main` 相对于5号参考版的差异仅有两处拾取接入逻辑。

完整项目的官方流程仍为：

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

若整合者用本轮 `member4_player_test.cpp` 替换整合版同名测试，并链接正式 `WorldState.cpp`，
需要为该测试目标定义 `MEMBER4_USE_REAL_WORLD`，关闭仅供4号独立分支使用的测试桩：

```cmake
target_compile_definitions(member4_tests PRIVATE MEMBER4_USE_REAL_WORLD)
```

本轮没有修改任何 CMake 文件；以上是整合时的目标配置说明。
验证脚本已经分别处理独立测试桩和正式 WorldState，两者不会在同一测试中重复定义。

## 保留的联调事项

- 房间物品的“不可再次获得”已复用主循环既有拾取旗标；但 `lookAround` 仍直接显示原始 `getItemIds()`。
  房间物品拾取后的场景移除仍需地图/状态模块使用正式公共状态接口完成。本轮没有改 `Room` 或场景 UI。
- 此4号分支的默认 CMake 是模块测试构建，不会构建依赖其他成员模块的 `src/main.cpp`；完整游戏验证使用上面的整合检查脚本。
- 无参数拾取教学、帮助界面更新仍留给界面负责人。没有改进度条、双栏、颜色或智慧成长。
