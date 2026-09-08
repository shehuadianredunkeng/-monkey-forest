# 3号最新联调摘要

## 今天新增或调整的接口

- NPC统一使用 `talk 中文名` 或 `talk 英文名`；不再向玩家提供 `quest`、`finish`。
- NPC对话选项直接输入裸数字，主循环调用
  `NPCSystem::chooseDialogueOption(option, ctx)`；不要输入 `talk 豆豆 2`。
- 第四次成功逃跑后若闪尾仍在队伍，主循环优先处理
  `flag_pending_scout_wander_choice`，并调用
  `CombatSystem::chooseEscapeEndingOption(option, ctx)`。
- 新增 `CollectionSystem`：统一注册、解锁、去重、计数和显示成就/结局。
- `getEndingCollectionText(world)` 与 `getAchievementCollectionText(world)` 可直接交给UI显示。
- `syncLegacyFlags(world)` 用于读档后兼容旧旗标，5号应在读档完成后调用一次。

## 新结局接入规范

其他成员新增结局时只需两步：

```cpp
collections.registerEnding({"ending_new_id", "中文结局名", "简短说明", false});
collections.unlockEnding("ending_new_id", ctx.world);
```

同一结局重复达成不会重复计数。隐藏结局在未解锁前显示为“？？？”。新增成就同理，分别调用
`registerAchievement` 和 `unlockAchievement`。

## 当前已收录

- 主结局：青木英雄、无声胜利、向南的新生、失落之谷。
- 坏结局：放火烧山、有了第一次就有第二次、你犯下了暴食罪。
- 隐藏结局：双宿双飞。
- 3号现有6个隐藏成就均已纳入成就收集接口。

## 各成员注意事项

- 1号：闪尾入队或离队后不再显示于果实森林；豆豆获救后不再显示于河谷。
- 2号：四个主结局结算时调用 `unlockEnding`，不要只设置路线旗标。
- 4号：裸数字的优先级为“闪尾隐藏邀请”高于“NPC等待选项”；可增加
  `结局（endings）`、`成就（achievements）`显示命令。
- 5号：普通进度旗标随存档保存；收集旗标建议单独永久保存，且读档后调用
  `syncLegacyFlags`。正式结局判定后也应调用 `unlockEnding`。
- 所有人：内部ID保持英文，玩家显示文字保持中文；不要直接修改其他模块私有状态。

## 兼容说明

旧结局旗标仍保留，没有破坏现有接口。`syncLegacyFlags`会把旧存档中的结局与成就补录到
新收集系统。收集器本身不负责写磁盘，最终持久化仍属于5号模块。

