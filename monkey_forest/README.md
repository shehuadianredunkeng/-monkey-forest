# -monkey-forest
超有趣的mud小游戏

## 构建与测试

需要安装 Visual Studio 的“使用 C++ 的桌面开发”工作负载，或其他支持 C++17 和 CMake
的工具链。

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

正式游戏入口为 `monkey_forest_game`。构建完成后可运行：

```powershell
.\build\Debug\monkey_forest_game.exe
```

进入游戏后可输入 `help` 查看命令表，输入 `guide` 查看下一步主线建议。常用命令包括
`look`、`go east`、`investigate`、`choose 1`、`talk npc_king`、`take item_fruit`、
`status`、`save`、`load`。

## 当前地图模块

`monkey_map` 提供 5 个房间的地图、`lookAround`、`movePlayer` 和命令帮助。主循环调用
`movePlayer` 后，只应在 `ActionResult.turnConsumed` 为 `true` 时消耗回合；地图层不会
直接调用 `WorldState` 结算回合。

## 当前剧情事件模块

`monkey_event` 提供6阶段开场、8个主线事件、4个随机事件和4种结局文本，
并通过统一旗标衔接NPC、战斗、玩家物品与后续存档系统。详细接口见
`docs/member2-integration-notes.md`。

## 当前状态与存档模块

`monkey_world` 提供正式 `WorldState`、`SaveManager`、`ProgressSystem`、`StatusView`
和 `EndingSystem`。4号主循环可以通过 `ProgressSystem::applyActionResult`
统一结算回合和阶段，通过 `buildStatusText` 响应 `status` 命令，通过 `SaveManager`
保存和读取进度，通过 `EndingSystem::determineEndingId` 获取最终结局 ID。
详细接口见 `docs/member5-integration-notes.md`。
