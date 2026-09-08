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

双栏界面使用 Windows Console 坐标渲染：左侧显示地点、剧情与操作记录，右侧固定为
任务、玩家状态、快捷命令三个区域，底部为独立输入栏。中线 X=73、右边框 X=110，
采用零基坐标，因此窗口至少需要 **111列、30行**。长命令不会挤动上方布局，
PageUp / PageDown 可以翻阅剧情记录。UI 不修改玩家、剧情、战斗或存档数据。

UI 验证与 Windows 原生截图操作见 [坐标 UI 接入说明](docs/coordinate-ui-integration.md)。
