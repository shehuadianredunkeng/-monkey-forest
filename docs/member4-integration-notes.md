# 4号玩家系统与主循环联调说明

## 一、负责范围

4号负责玩家属性、技能、背包、物品使用和游戏主循环，并承担最终界面的输入与显示衔接。地图数据和房间连接归1号，主线剧情归2号，NPC与战斗规则归3号，存档底层和结局判定归5号。

## 二、4号修改的文件

### 1. 玩家与物品核心文件

| 文件 | 修改内容 |
| --- | --- |
| `include/Item.h`、`src/Item.cpp` | 定义物品编号、名称、类型和使用效果等基础数据。 |
| `include/Inventory.h`、`src/Inventory.cpp` | 实现背包添加、移除、查找、堆叠与容量检查；容量统一由 `Inventory::MAX_SLOTS` 管理，当前为12格。 |
| `include/Player.h`、`src/Player.cpp` | 管理生命、体力、力量、智慧、声望、技能等级和背包等玩家状态。 |
| `include/PlayerActions.h`、`src/PlayerActions.cpp` | 处理拾取、使用、丢弃、休息、训练等玩家操作，并通过 `ActionResult` 返回结果及是否消耗回合。 |

### 2. 主循环与界面联调文件

| 文件 | 修改内容 |
| --- | --- |
| `src/main.cpp` | 串联地图、剧情、NPC、战斗、玩家、存档和结局；统一回合推进、战斗中背包及存档入口。 |
| `include/UI/GameUI.h`、`src/UI/GameUI.cpp` | 调整剧情记录、选项颜色和界面输出；选项统一显示为黄色。 |
| `src/UI/InteractiveGameUI.cpp` | 实现WASD移动、上下键回看剧情、切图体力消耗、右栏常驻状态与背包、全屏结局字幕等。 |
| `.gitignore` | 忽略本地构建和调试产生的临时文件。 |

### 3. 测试与说明文件

| 文件 | 修改内容 |
| --- | --- |
| `tests/member4_player_test.cpp` | 测试玩家属性、技能、背包与物品行为。 |
| `tests/member4_main_test.cpp` | 测试4号模块与主循环的基础衔接。 |
| `tests/TestSupport.cpp` | 为4号模块测试提供公共桩和辅助实现。 |
| `tests/ui_layout_test.cpp` | 检查最终UI布局、输入规则和结局显示逻辑。 |
| `tests/run_member4_checks.sh` | 4号早期模块检查脚本。 |
| `docs/member-4-player.md` | 4号玩家模块使用说明。 |
| `docs/member4-integration-notes.md` | 本联调说明。 |
| `CMakeLists.txt` | 注册4号源码和测试目标；合并时应保留其他成员已经加入的构建项。 |

## 三、最终一轮的实际净改动

在合并1、2、3号代码后，4号最终提交实际留下改动的文件为：

- `.gitignore`
- `docs/member4-integration-notes.md`
- `include/UI/GameUI.h`
- `src/UI/GameUI.cpp`
- `src/UI/InteractiveGameUI.cpp`
- `src/main.cpp`
- `tests/ui_layout_test.cpp`

`src/Room.cpp` 最终无净改动。该文件保留在项目中并由1号维护，合并4号分支时不要删除或用旧版本覆盖。

## 四、主要功能变化

- 探索移动只使用 `W/A/S/D`；上下箭头逐行翻阅剧情，`PageUp/PageDown` 快速翻页。
- 只有成功切换房间才固定消耗3点体力；房间内走动不扣体力，体力不足时禁止切图。
- 右侧状态栏常驻显示生命、体力、力量、智慧、声望、公共资源、四项技能和背包物品。
- 结局进入独立全屏谢幕界面，不再被地图或状态框遮挡；字幕逐行显示，间隔1秒，结束后可上下回看。
- 剧情和NPC的数字选项统一使用黄色，避免同一组选项颜色不一致。
- 战斗中允许查看背包并使用物品，也允许存档。
- 界面不再提示玩家输入 `guide`，当前任务直接显示在右栏。
- 背包使用12格上限；同编号物品自动堆叠，拾取、使用和丢弃继续由玩家动作模块处理。

## 五、调用其他成员的接口

| 对接成员 | 4号调用的接口/数据 | 约定 |
| --- | --- | --- |
| 1号地图 | `InteractiveMap` 的移动、当前房间和交互结果 | 4号只处理输入、显示和切图扣体力，不修改房间连接与地形。 |
| 2号剧情 | `EventSystem::getCurrentObjective(ctx)`、`chooseEventOption("", option, ctx)`、`resumePendingEventAfterBattle(ctx)` | 主线文本、选项结果和智慧成长路线仍以2号实现为准。 |
| 3号NPC/战斗 | NPC交互、战斗开始与结算、成就和结局收集接口 | 4号只提供战斗输入入口及结果显示，不改敌人数值和NPC剧情。 |
| 5号存档/结局 | `SaveSlots`、`SaveManager`、结局判定与展示数据 | 继续使用统一存档格式，不建立第二套玩家或剧情状态。 |

公共约定：只有 `ActionResult.turnConsumed == true` 才推进回合。显示层不得直接改玩家、剧情、NPC或地图状态。

## 六、推荐合并顺序

1. 先合并1号地图分支。
2. 再合并2号剧情分支。
3. 再合并3号NPC与战斗分支。
4. 合并4号玩家与主循环分支。
5. 最后由5号补齐存档、结局判定并进行全项目测试。

若发生冲突，按以下归属保留：

- `Room.cpp`、房间连接和地图地形：保留1号版本。
- `StoryText`、`EventSystem` 和主线原文：保留2号版本。
- NPC、敌人、战斗、成就与结局收集：保留3号版本。
- `Player`、`Inventory`、`PlayerActions`、最终UI输入与主循环：保留4号实现，并手动补入其他成员新增调用。
- 存档序列化和最终结局判定：保留5号版本。

不要直接选择“全部保留某一边”解决 `main.cpp`、`CMakeLists.txt` 或UI冲突，这些文件同时承担多模块注册，必须逐段合并。

## 七、已知限制

- 战斗中存档会保存玩家、地图和剧情状态，但目前不会保存敌人的剩余生命；读档后该敌人会以完整生命重新进入战斗。
- 右栏背包空间有限，物品过多时只显示摘要，玩家可进入完整背包界面查看。
- 4号分支包含已合入的1、2、3号代码快照，但这些文件不代表由4号维护；判断责任时以本说明的文件边界为准。

## 八、验证结果

Visual Studio 2022 Release 构建成功，以下8组测试全部通过：

- `ui_layout_tests`
- `map_tests`
- `member4_tests`
- `member3_tests`
- `member2_tests`
- `member5_tests`
- `interactive_map_tests`
- `save_slots_tests`

其中 `member2_tests` 已验证：即使树冠试炼没有选择智慧奖励，后续研究事件仍可使智慧达到4。

## 九、对应提交

- `61b7398`：完成最终UI、输入和主循环联调。
- `215bfa1`：撤销4号对 `Room.cpp` 的改动，将地图文件完整交还1号维护。
