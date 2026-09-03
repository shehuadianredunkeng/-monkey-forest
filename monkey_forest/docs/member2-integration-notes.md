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

## 2026-09-01 智慧成长防锁死修复

基线：`member-5-zip` 的 `9afe9d302318977bd4e8273bac4b2600c17496f9`，
实际工程位于仓库的 `monkey_forest/` 子目录。修复分支为 `fix/wisdom-growth`，
PR 目标为 `member-5-zip`，由5号审核整合，不自动合并。

### 五个机会，任意三个达到智慧4

玩家初始智慧1，上限5。以下是五个独立、一次性的成长节点，不是六个：

| 节点 | 原有事件与选项 | 首次领取状态 |
|---|---|---|
| 树冠观察 | `event_tree_trial` 第3项 | 新增 `flag_wisdom_tree` |
| 河谷抽水规律 | `event_glowing_river` 第2项 | 新增 `flag_wisdom_river` |
| 山洞刻痕与回声 | `event_echo_tracking` 第1项 | 新增 `flag_wisdom_cave` |
| 晶片技术结构 | `event_drought_choice` 第2项，或 `event_drone_crash` 第1项 | 复用 `flag_route_hack_ready` 或 `flag_drone_analyzed`，任意一个已设置即不再奖励 |
| 基地日志研究 | `event_base_infiltration` 第2项，或从其他合法方案取得完整日志后研究 | 新增 `flag_wisdom_base` |

四个新旗标及奖励元数据集中定义在 `EventSystem.cpp` 的匿名命名空间内。
没有新增 `flag_wisdom_chip`，没有新增公共接口或状态管理器。

`flag_event_tree_trial_done` / `flag_event_echo_tracking_done` 也能由非智慧方案设置，
不能表示理解过对应规律。旧 `flag_water_clue` 在低智慧、没有获得奖励时也会设置。
`flag_complete_log` 在战斗、潜入、同伴三条基地路径都会设置，表示取得日志，
不表示完成研究。因此不能用这些旗标替代对应的智慧领取记录。

两个晶片研究选项继续分别记录各自的正式剧情结果；奖励检查使用二者的逻辑或。
例如先研究侦察机，再选择干旱事件的晶片研究，仍可完成主线准备，但不会再加智慧。
单纯持有晶片或 `flag_chip_found` 都不代表已经研究。研究始终保留真实 `item_chip`。

### 玩家入口与补充研究

沿用现有 `investigate` / `调查` 和 `choose` / `选择` 命令：

| 输入示例 | 条件与行为 |
|---|---|
| `investigate 树冠` | 原试炼未结束时打开原事件；其他方案结束试炼后不能重新选观察，但主线照常推进 |
| `investigate 管线` / `investigate 银色管线` | 第3阶段起，在河谷调查；未完成河谷主线时打开原事件，完成水源事件后可补充研究 |
| `investigate 碑文` / `investigate 回声` | 第3阶段起，在山洞调查；未完成山洞主线时打开原事件，完成后仍可补充研究 |
| `investigate 晶片` / `investigate 星猿晶片` | 第2阶段起，持有真实晶片即可研究；不依赖侦察机随机事件名额，不消耗晶片 |
| `investigate 日志` / `investigate 控制台` | 第5阶段起，在已开放基地、持有晶片且取得完整日志后研究；没有额外智慧门槛 |

河谷、山洞和晶片研究已去除“先有智慧才能增加智慧”的门槛。
基地原有的晶片伪装权限方案仍要求智慧至少3；战斗或同伴路线拿到日志后，
智慧1也可以研究日志。因此不需要降低潜入条件，就能补足基地成长机会。

补充研究不重开主线，不重复发物资、晶片或声望，不推进阶段、不清除其他待选事件。
首次研究返回成功并消耗一个行动回合，回合仍由 `ProgressSystem` 统一结算。
重复调查只说明已经理解，不加属性、不消耗回合。达到智慧5时仍记录领取状态，
但提示已达上限，不显示虚假的“【智慧 +1】”。

最终抉择已经触发、但因智慧不足尚未选定结局时，仍可以回到河谷或山洞补查，
再回猴王树继续原来的选择。正式结局已确定后不再允许研究奖励。

### 存档与旧存档

`SaveManager` 原有版本1格式已经逐项保存、恢复 `WorldState::getFlags()` 的全部字符串旗标，
本次不修改 `SaveManager`、`WorldState` 或存档版本。奖励智慧与领取状态一起保存。

旧存档没有四个新旗标时按未设置处理，不崩溃，不猜测历史选项，也不自动调整已有智慧。
旧剧情若已经发过智慧、但没有独立领取记录，不能从现有数据可靠还原；仍按缺失旗标未领取处理。
这可能允许旧进度补领一个此前奖励过的研究节点，但补领后立即持久记录，不能持续刷点。
已有 `flag_drone_analyzed` 或 `flag_route_hack_ready` 的旧存档仍会阻止晶片节点重复奖励。
树冠已经以其他方案结束的旧进度不会重新开放试炼，河谷／山洞／晶片／基地仍可补足成长。

### 保持不变

- `Player::getWisdom()` / `changeWisdom(int)`，以及1～5范围，不改实现。
- `SkillType` 仍为采集、攀爬、战斗、领导；拒绝 `train wisdom` 和 `train 智慧`。
- `GameContext`、事件系统和 `trainSkill` 的公共签名不变。
- 智取事件仍要求智慧至少4及完整日志，成功后才设置系统破解、智取选择和最终选择旗标。
- 不要求五节点全收集，也不要求树冠智慧旗标；基地开放、晶片和其他剧情条件不降低。
- 不修改 `main.cpp`、PlayerActions、Inventory、NPC、Combat、Room 或其他成员分支。
- `StatusView.cpp` 仅过滤新增的 `flag_wisdom_` 前缀，避免内部领取记录泄露给玩家；
  不改布局、不清理既有旗标显示、不修改状态接口。

### 回归测试与构建

新增 `tests/wisdom_growth_test.cpp`（19组）和 `tests/wisdom_main_test.cpp`（6组）。
后者运行真实 `src/main.cpp` 的命令循环，没有替代解析器或伪造阶段：

- 全部10种“三选三”组合均从新游戏推进至智取结局，包含跳过树冠以及同时跳过树冠与河谷。
- 每个节点首次奖励、重复提示、达到上限后领取，以及新建状态／事件系统后的存档恢复。
- 侦察机和晶片两种研究顺序只加一点，晶片始终保留；随机事件名额用完仍可研究持有的晶片。
- 错误选项、体力不足和背包满后的重试；失败不提前发智慧或标记研究完成。
- 基地原有潜入门槛保留，潜入与补查日志共享奖励。
- 真实 `save → quit → 新游戏进程入口 → load → 重复调查` 不再加智慧。
- 已触发最终抉择后被智慧门槛拒绝，回头补查三个节点，仍能继续原选择得到智取结局。
- 旧格式缺少新旗标仍可加载；Player极端整数增减不越界。

正常CMake流程（新增两项测试已注册）：

```sh
cmake -S monkey_forest -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

没有CMake时，从仓库根目录使用直接构建脚本：

```sh
bash monkey_forest/tests/run_wisdom_checks.sh /absolute/path/outside-repo/wisdom-build
WISDOM_SANITIZE=1 bash monkey_forest/tests/run_wisdom_checks.sh /absolute/path/outside-repo/wisdom-ubsan
```

脚本以 `-std=c++17 -Wall -Wextra -Wpedantic -Werror` 编译完整游戏和全部7个测试程序，
临时存档与构建产物不进入源码目录。实际验证环境缺少CMake，使用GCC直接构建；
不能把这一结果写成已经运行过CMake/CTest。

普通构建与UBSan构建均通过全部7个测试程序。主循环存档测试使用独立临时工作目录和
无空格的存档文件名，并断言确实写入目标文件；临时目录含空格也不会把路径截断后误测。

### 给2号／4号／5号的整合提醒

- 2号：确认上述五个既有剧情选项和四个专用旗标；合并后不再保留旧的六处独立智慧加点。
- 4号：玩家接口、训练和背包无需改动。本分支不包含4号另一分支上的拾取修复。
- 5号：在 `monkey_forest/` 目录整合本PR；将4号拾取修复作为独立变更接入，
  不要以本分支未改动的旧 `main.cpp` 覆盖其他已经整合的主循环修改。
- 已知既有问题（不属于本次奖励重复修复）：同一运行实例加载不同待选事件的存档时，
  EventSystem缓存的活动事件可能暂时与存档不一致；重新调查对应事件可恢复。
  新建实例加载恢复正常。本轮没有顺手重构这套待选缓存，后续由2号／5号单独确认。
