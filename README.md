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

当前版本整合了各成员的职责代码：

- 1号：五张地图、动态场景、出口和地图显示、ui自适应。
- 2号：第一年“树冠试炼”主线以及后续事件接口。
- 3号：NPC对话、战斗系统、成就和结局润色收集。
- 4号：玩家属性、背包、物品、训练和主循环交互。
- 5号：状态、阶段推进、存档读取和结局判断。

Windows下构建后运行：

```powershell
cmake -S . -B build
cmake --build build --config Debug
.\build\Debug\monkey_forest_interactive.exe
```

双栏界面使用 Windows Console 坐标渲染：左侧显示地点、剧情与操作记录，右侧固定为
任务、玩家状态、快捷命令三个区域，底部为独立输入栏。中线 X=73、右边框 X=110，
采用零基坐标。
PageUp / PageDown 可以翻阅剧情记录，↓ 直接回到最新一条。

玩家技能为攀爬、战斗和领导。战斗技能初始为 1 级，后续升级依次需要
5、6、7、8 场新胜利，最高 5 级。
每次战斗技能升级恢复 50 点生命。战斗中存档会同时保留敌人生命和战斗状态。

UI 验证与 Windows 原生截图操作见 [坐标 UI 接入说明](docs/coordinate-ui-integration.md)。
