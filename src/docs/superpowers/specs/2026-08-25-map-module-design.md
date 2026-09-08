# 地图模块设计

## 范围

本模块实现 MUD 游戏的地图与房间层，负责房间定义、房间连接、场景描述、
`look`、`go`、出口检查和命令帮助。玩家状态、事件、NPC、战斗、存档、阶段推进
及正式游戏主循环均不在本模块职责范围内。

## 构建与目录结构

项目使用 CMake 与 C++17。公共接口放在 `include/`，地图实现放在 `src/`，测试
放在 `tests/`。地图代码会被构建为库，后续的 `Game` 和主循环可以链接该库，但
本模块不占用游戏程序入口。

## 公共接口约定

`CommonTypes.h` 定义共享的 `ActionResult` 和 `GameContext` 类型；`Room.h` 定义
`Room` 值对象，包含房间 id、名称、基础描述、出口、NPC id 和物品 id。本模块提供：

- `std::map<std::string, Room> createAllRooms()`：创建 5 个标准房间。
- `ActionResult movePlayer(GameContext&, const std::string& direction)`：执行带校验的移动。
- `std::string lookAround(const GameContext&)`：查看当前房间场景。
- `std::string getCommandHelp()`：返回命令帮助文本。

`GameContext` 引用由后续游戏层持有的 `Player`、`WorldState` 和房间表。地图层读取
玩家当前房间与体力，只在移动成功时更新必要状态，并返回 `ActionResult`；主循环仍
负责根据结果消耗回合，避免重复计数。

## 地图数据与行为

初始地图包含设计文档规定的房间 id：`room_tree`、`room_forest`、`room_river`、
`room_cave`、`room_base`。连接关系以“方向 -> 目标房间 id”保存。普通移动消耗 1 点
体力；当玩家攀爬技能达到 2 级时，可从森林使用通往河谷的树冠捷径且不消耗体力。
方向不合法、出口不存在、目标房间未知或体力不足时，函数应安全失败，给出说明信息，
且不改变游戏状态。

`lookAround` 输出当前房间名称和描述、可用出口、可见 NPC、可见物品及简短推荐行动；
如果玩家所在房间不在地图中，应返回便于恢复问题的提示，而不是抛出异常。

## 联调规则

本模块使用小组接口文档中统一的 id，不直接调用事件、NPC、战斗、存档、结局或 UI
系统。后续模块可以通过 `WorldState` 标志扩充场景描述，但不得改变公开的房间地图接口。
4 号的主循环调用 `movePlayer` 后，仅依据 `ActionResult.turnConsumed` 决定是否消费回合；
5 号可以通过 `WorldState` 扩展状态，但不应让地图层直接结算回合。

## 测试

单元测试链接地图库，验证 5 个房间均存在、关键连接可达、合法移动会更新房间和结果
标志、非法移动不会修改状态、攀爬捷径符合体力消耗规则，以及 `lookAround` 和帮助文本
能提供可执行的指引。不提供正式的 `main` 函数。
