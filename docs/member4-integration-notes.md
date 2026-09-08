# 4号模块最终联调说明

## 本次完成内容

- 探索移动只使用 `W/A/S/D`，上下箭头逐行翻阅剧情，`PageUp/PageDown` 快速翻页。
- 成功切换房间固定消耗 3 点体力；房间内走动不消耗体力，体力不足 3 点时禁止切图。
- 结局使用全屏谢幕界面，不显示地图和状态栏；字幕每行间隔 1 秒，结束后可以上下回看。
- 所有以 `1.`、`2.`、`1、` 等格式开头的剧情或 NPC 选项统一使用黄色提示色。
- 右侧状态区常驻显示生命、体力、力量、智慧、声望、公共资源、四项技能和背包物品。
- 战斗中支持 `背包/inventory/bag` 和 `存档/save`。战斗存档记录玩家、地图和剧情状态；读档后敌人仍在原处，可重新挑战。
- 玩家界面和房间提示不再要求输入 `guide`，主线目标直接显示在右侧。
- 背包容量统一使用 `Inventory::MAX_SLOTS`，当前为 12 格；同 ID 物品堆叠。

## 保持不变的公共接口

- 玩家属性和技能继续通过 `Player` 的公开 getter 与 change 接口访问。
- 背包通过 `Player::getInventory()` 只读展示，拾取和使用仍调用 `PlayerActions`。
- 主线目标由 `EventSystem::getCurrentObjective(ctx)` 提供。
- 事件选择调用 `EventSystem::chooseEventOption("", option, ctx)`。
- 战斗完成后调用 `EventSystem::resumePendingEventAfterBattle(ctx)`。
- 只有 `ActionResult.turnConsumed == true` 时才推进回合。
- 存档继续通过 `SaveSlots` 与 `SaveManager`，未增加第二套存档格式。

## 与其他成员的边界

- 2号继续维护主线事件、结局原文和智慧成长路径；4号只负责显示、输入和主循环衔接。
- 3号继续维护 NPC、敌人、战斗结算、成就条件；4号只开放战斗中的背包和存档入口。
- 5号继续维护完整存档内容和结局判定。当前战斗内部的敌方剩余生命不会写入存档，因此读档后从该敌人的完整战斗重新开始。
- 1号继续维护地图地形和房间连接；4号只执行切图体力规则。

## 验证结果

Visual Studio 2022 Release 构建成功，以下 8 组测试全部通过：

- `ui_layout_tests`
- `map_tests`
- `member4_tests`
- `member3_tests`
- `member2_tests`
- `member5_tests`
- `interactive_map_tests`
- `save_slots_tests`

其中 `member2_tests` 已验证：即使树冠试炼未选择智慧奖励，后续河谷研究、回声研究和日志研究仍可使智慧达到 4。
