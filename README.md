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

## 当前地图模块

`monkey_map` 提供 5 个房间的地图、`lookAround`、`movePlayer` 和命令帮助。主循环调用
`movePlayer` 后，只应在 `ActionResult.turnConsumed` 为 `true` 时消耗回合；地图层不会
直接调用 `WorldState` 结算回合。

## 第一年度双栏UI试玩版

`first-year-ui-demo`分支整合了各成员的最新职责代码：

- 1号：五张地图、动态场景、出口和地图显示。
- 2号：第一年“树冠试炼”主线以及后续事件接口。
- 3号：NPC对话、野蜂战斗、成就和结局收集。
- 4号：玩家属性、背包、物品、训练和主循环交互。
- 5号：状态、阶段推进、存档读取和结局判断。

Windows下构建后运行：

```powershell
cmake -S . -B build
cmake --build build --config Debug
.\build\Debug\monkey_forest_first_year.exe
```

试玩目标：从猴王树前往果实森林，完成第一年的树冠试炼。进入正确地点会自动触发
主线，出现选项时直接输入数字即可。也可以先探索、拾取物品或完成闪尾任务。

双栏界面固定为120列：左侧显示剧情与操作记录，右侧显示地图、状态、当前任务和快捷
命令，底部为输入栏。推荐使用Windows Terminal并保持窗口宽度不小于120列。
