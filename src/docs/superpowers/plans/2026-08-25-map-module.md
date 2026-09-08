# 地图模块实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建可独立编译、测试并供主循环接入的五房间地图模块。

**Architecture:** 使用 CMake 构建 `monkey_map` 静态库。公共头文件定义统一结果类型、最小跨模块声明和 `Room`，实现文件只处理地图数据、观察、移动和帮助；测试目标用测试桩提供 `Player` 的行为定义，避免实现 4、5 号成员负责的正式逻辑。

**Tech Stack:** C++17、CMake、标准库、无第三方依赖。

**Spec:** `docs/superpowers/specs/2026-08-25-map-module-design.md`

## 全局约束

- 使用 C++17 和 CMake，不引入第三方库。
- 统一使用接口文档中的房间、NPC、物品与技能 id。
- 只有主循环根据 `ActionResult::turnConsumed` 消耗回合；地图模块不得调用 `WorldState::consumeTurn`。
- 不创建正式 `main`，不实现事件、NPC、战斗、存档、结局或正式玩家业务逻辑。
- 所有状态改变型操作返回 `ActionResult`，失败时不改变玩家位置和体力。

---

### Task 1: 构建骨架与跨模块声明

**Files:**
- Create: `CMakeLists.txt`
- Create: `include/CommonTypes.h`
- Create: `include/Player.h`
- Create: `include/WorldState.h`
- Create: `tests/TestSupport.cpp`
- Create: `tests/TestFramework.h`
- Create: `tests/TestMain.cpp`

**Interfaces:**
- Produces: `ActionResult`、`SkillType`、`ResourceType`、`GameContext`、`Player` 和 `WorldState` 的跨模块声明。
- Produces: `monkey_map` 静态库目标和 `map_tests` 测试目标。

- [ ] **Step 1: 写出会失败的构建配置测试**

创建 `CMakeLists.txt`，先只声明以下测试目标，使配置阶段因缺少源文件失败：

```cmake
cmake_minimum_required(VERSION 3.16)
project(monkey_forest LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_library(monkey_map src/Room.cpp)
target_include_directories(monkey_map PUBLIC include)
add_executable(map_tests tests/TestMain.cpp tests/TestSupport.cpp tests/RoomTests.cpp)
target_link_libraries(map_tests PRIVATE monkey_map)
enable_testing()
add_test(NAME map_tests COMMAND map_tests)
```

- [ ] **Step 2: 运行配置，确认失败**

运行：`cmake -S . -B build`

预期：失败信息指出 `src/Room.cpp`、`tests/TestMain.cpp`、`tests/TestSupport.cpp` 或 `tests/RoomTests.cpp` 尚不存在。

- [ ] **Step 3: 实现最小构建骨架和共享声明**

创建空的 `src/Room.cpp`、`tests/RoomTests.cpp`，并实现如下最小公共定义：

```cpp
enum class SkillType { Gather, Climb, Combat, Leadership };
struct ActionResult {
    bool success = false;
    std::string message;
    bool turnConsumed = false;
    bool stageCompleted = false;
};
class WorldState {};
class Player {
public:
    int getStamina() const;
    void changeStamina(int delta);
    int getSkillLevel(SkillType skill) const;
    std::string getCurrentRoomId() const;
    void setCurrentRoomId(const std::string& roomId);
};
```

在 `CommonTypes.h` 中定义 `GameContext`：

```cpp
struct GameContext {
    Player& player;
    WorldState& world;
    std::map<std::string, Room>& rooms;
};
```

在 `TestSupport.cpp` 中为测试目标定义这些 `Player` 方法，状态保存在仅测试可见的表中；正式实现由 4 号成员替换。`TestFramework.h` 提供 `TEST_ASSERT(condition)`，失败时抛出 `std::runtime_error`；`TestMain.cpp` 运行 `runRoomTests()` 并返回非零失败码。

- [ ] **Step 4: 运行构建与空测试目标，确认通过**

运行：`cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`

预期：配置、构建和 `map_tests` 均成功。

- [ ] **Step 5: 提交构建骨架**

```bash
git add CMakeLists.txt include/CommonTypes.h include/Player.h include/WorldState.h src/Room.cpp tests
git commit -m "build: add map module skeleton"
```

### Task 2: 房间值对象与五房间地图

**Files:**
- Create: `include/Room.h`
- Modify: `src/Room.cpp`
- Modify: `tests/RoomTests.cpp`

**Interfaces:**
- Consumes: `ActionResult`、`GameContext` 和测试框架。
- Produces: `Room`、`createAllRooms()`、`lookAround()` 和 `getCommandHelp()`。

- [ ] **Step 1: 写出地图与观察的失败测试**

在 `tests/RoomTests.cpp` 编写测试：`createAllRooms()` 的结果大小为 5，且包含
`room_tree`、`room_forest`、`room_river`、`room_cave`、`room_base`；猴王树有通往森林的出口；玩家位于 `room_tree` 时 `lookAround(context)` 包含房间名、`room_forest` 或“森林”、以及“出口”；`getCommandHelp()` 包含 `look`、`go` 和 `help`。

- [ ] **Step 2: 运行测试，确认失败**

运行：`cmake --build build && ctest --test-dir build --output-on-failure`

预期：编译失败，提示 `Room`、`createAllRooms`、`lookAround` 或 `getCommandHelp` 未定义。

- [ ] **Step 3: 实现房间模型、地图和文本输出**

在 `include/Room.h` 定义 `Room`，保存 id、名称、基础描述、`std::map<std::string, std::string>` 出口、NPC id 和物品 id，并提供 const getter。在 `src/Room.cpp` 初始化以下连接：

```text
room_tree <-> room_forest
room_forest <-> room_river
room_forest <-> room_cave
room_river <-> room_base
room_cave <-> room_base
```

方向字符串使用 `north`、`south`、`east`、`west` 和 `up`。在每个房间写入中文场景描述、可见 NPC/物品和推荐行动。`lookAround` 对未知当前房间返回“当前位置无效，请返回猴王树重新开始。”；正常输出必须列出出口、NPC 和物品。`getCommandHelp()` 列出 `look`、`go <direction>`、`talk <npc_id>`、`take <item_id>`、`status` 和 `help` 的用途。

- [ ] **Step 4: 运行地图与观察测试，确认通过**

运行：`cmake --build build && ctest --test-dir build --output-on-failure`

预期：所有地图、观察和帮助断言通过。

- [ ] **Step 5: 提交房间地图**

```bash
git add include/Room.h src/Room.cpp tests/RoomTests.cpp
git commit -m "feat: add five-room map and look command"
```

### Task 3: 带出口和体力校验的移动

**Files:**
- Modify: `include/Room.h`
- Modify: `src/Room.cpp`
- Modify: `tests/RoomTests.cpp`

**Interfaces:**
- Consumes: `GameContext::player`、`Player::getStamina()`、`Player::changeStamina()`、`Player::getSkillLevel()`、`Player::getCurrentRoomId()` 和 `Player::setCurrentRoomId()`。
- Produces: `ActionResult movePlayer(GameContext&, const std::string& direction)`。

- [ ] **Step 1: 写出移动的失败测试**

新增测试：从 `room_tree` 执行 `movePlayer(context, "east")` 成功后，玩家位置变为 `room_forest`、体力减少 1、`success` 与 `turnConsumed` 为 true；不存在的方向失败且位置体力不变；体力为 0 时正常移动失败；从 `room_forest` 使用通往河谷的 `up` 出口时，攀爬等级 1 消耗 1 体力，攀爬等级 2 不消耗体力。

- [ ] **Step 2: 运行移动测试，确认失败**

运行：`cmake --build build && ctest --test-dir build --output-on-failure`

预期：编译失败，提示 `movePlayer` 未声明或未定义。

- [ ] **Step 3: 实现最小移动逻辑**

在 `Room.h` 声明：

```cpp
ActionResult movePlayer(GameContext& context, const std::string& direction);
```

在 `Room.cpp` 依次检查当前房间存在、方向非空且存在出口、目标房间存在、体力是否满足。普通成功移动调用 `changeStamina(-1)` 并设置新房间；仅 `room_forest` 经 `up` 到 `room_river` 且 `getSkillLevel(SkillType::Climb) >= 2` 时不扣体力。所有失败路径返回 `success == false`、`turnConsumed == false` 且不修改玩家；成功路径返回 `success == true`、`turnConsumed == true`、`stageCompleted == false`。

- [ ] **Step 4: 运行完整测试，确认通过**

运行：`cmake --build build && ctest --test-dir build --output-on-failure`

预期：全部测试通过。

- [ ] **Step 5: 提交移动实现**

```bash
git add include/Room.h src/Room.cpp tests/RoomTests.cpp
git commit -m "feat: add validated room movement"
```

### Task 4: 最终验证与协作交接

**Files:**
- Modify: `README.md`
- Modify: `docs/superpowers/specs/2026-08-25-map-module-design.md`

**Interfaces:**
- Consumes: 完成的 `monkey_map` 库。
- Produces: 本地构建说明和 4、5 号成员可直接采用的调用约定。

- [ ] **Step 1: 写出 README 中应出现的构建命令和模块边界检查**

在 `README.md` 增加以下文本并将它视为文档验收条件：

```text
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

还应说明地图层不消耗 `WorldState` 回合，主循环仅在 `ActionResult.turnConsumed` 为 true 时消费回合。

- [ ] **Step 2: 运行最终验证**

运行：`cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure && git status --short`

预期：构建和测试成功，除 README 与设计说明外没有未跟踪或未提交的源代码变更。

- [ ] **Step 3: 补充构建说明与接口交接说明**

在 `README.md` 加入 CMake 构建命令和 `monkey_map` 的功能列表。在设计说明的“联调规则”后补充：4 号主循环调用 `movePlayer` 后仅依赖 `ActionResult` 决定是否消费回合；5 号可以通过 `WorldState` 扩展状态但不应让地图层直接结算回合。

- [ ] **Step 4: 再次运行最终验证**

运行：`cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`

预期：构建与全部测试通过。

- [ ] **Step 5: 提交并推送**

```bash
git add README.md docs/superpowers/specs/2026-08-25-map-module-design.md
git commit -m "docs: add map module integration guide"
git push origin main
```
