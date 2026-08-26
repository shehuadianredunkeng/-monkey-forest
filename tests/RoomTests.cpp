#include "Room.h"
#include "TestFramework.h"
#include "TestSupport.h"
#include "WorldState.h"

#include <algorithm>

void testMapContainsFiveCanonicalRooms() {
    const auto rooms = createAllRooms();

    assertTrue(rooms.size() == 5, "地图应包含五个房间");
    assertTrue(rooms.count("room_tree") == 1, "缺少猴王树");
    assertTrue(rooms.count("room_forest") == 1, "缺少果实森林");
    assertTrue(rooms.count("room_river") == 1, "缺少清泉河谷");
    assertTrue(rooms.count("room_cave") == 1, "缺少回声山洞");
    assertTrue(rooms.count("room_base") == 1, "缺少废弃实验基地");
    assertTrue(rooms.at("room_tree").getExits().count("east") == 1,
               "猴王树应有通往森林的东侧出口");
}

void testCaveContainsChipBeforeBaseInfiltration() {
    const auto rooms = createAllRooms();
    const auto& caveItems = rooms.at("room_cave").getItemIds();
    const auto& baseItems = rooms.at("room_base").getItemIds();

    assertTrue(std::find(caveItems.begin(), caveItems.end(), "item_chip") != caveItems.end(),
               "回声山洞应提供进入基地所需的晶片");
    assertTrue(std::find(baseItems.begin(), baseItems.end(), "item_chip") == baseItems.end(),
               "基地内部不应放置用于开启自身的晶片");
}

void testLookDescribesCurrentRoomAndExits() {
    Player player;
    player.setCurrentRoomId("room_tree");
    WorldState world;
    auto rooms = createAllRooms();
    GameContext context{player, world, rooms};

    const std::string scene = lookAround(context);

    assertTrue(scene.find("猴王树") != std::string::npos, "观察应显示当前房间名称");
    assertTrue(scene.find("森林") != std::string::npos, "观察应提示可到达的森林");
    assertTrue(scene.find("出口") != std::string::npos, "观察应列出出口");
}

void testHelpListsCoreCommands() {
    const std::string help = getCommandHelp();

    assertTrue(help.find("look") != std::string::npos, "帮助应包含 look 命令");
    assertTrue(help.find("go") != std::string::npos, "帮助应包含 go 命令");
    assertTrue(help.find("help") != std::string::npos, "帮助应包含 help 命令");
}

void testValidMoveChangesRoomAndConsumesStamina() {
    Player player;
    player.setCurrentRoomId("room_tree");
    setTestPlayerStamina(player, 3);
    WorldState world;
    auto rooms = createAllRooms();
    GameContext context{player, world, rooms};

    const ActionResult result = movePlayer(context, "east");

    assertTrue(result.success, "合法出口应允许移动");
    assertTrue(result.turnConsumed, "成功移动应请求主循环消耗回合");
    assertTrue(player.getCurrentRoomId() == "room_forest", "移动后应位于目标房间");
    assertTrue(player.getStamina() == 2, "普通移动应消耗一点体力");
}

void testInvalidMoveLeavesPlayerStateUnchanged() {
    Player player;
    player.setCurrentRoomId("room_tree");
    setTestPlayerStamina(player, 3);
    WorldState world;
    auto rooms = createAllRooms();
    GameContext context{player, world, rooms};

    const ActionResult result = movePlayer(context, "north");

    assertTrue(!result.success, "不存在的出口应失败");
    assertTrue(!result.turnConsumed, "失败移动不应消耗回合");
    assertTrue(player.getCurrentRoomId() == "room_tree", "失败移动不能改变位置");
    assertTrue(player.getStamina() == 3, "失败移动不能消耗体力");
}

void testMoveFailsWhenStaminaIsEmpty() {
    Player player;
    player.setCurrentRoomId("room_tree");
    setTestPlayerStamina(player, 0);
    WorldState world;
    auto rooms = createAllRooms();
    GameContext context{player, world, rooms};

    const ActionResult result = movePlayer(context, "east");

    assertTrue(!result.success, "体力不足时不应移动");
    assertTrue(player.getCurrentRoomId() == "room_tree", "体力不足时应保留原位置");
}

void testBaseEntranceRequiresOpenFlag() {
    Player player;
    player.setCurrentRoomId("room_river");
    setTestPlayerStamina(player, 3);
    WorldState world;
    auto rooms = createAllRooms();
    GameContext context{player, world, rooms};

    const ActionResult result = movePlayer(context, "east");

    assertTrue(!result.success, "基地未解锁时不应允许进入");
    assertTrue(player.getCurrentRoomId() == "room_river", "门禁拦截后应保留原位置");
    assertTrue(player.getStamina() == 3, "门禁拦截不应消耗体力");
}

void testOpenBaseFlagAllowsEntry() {
    Player player;
    player.setCurrentRoomId("room_river");
    setTestPlayerStamina(player, 3);
    WorldState world;
    setTestWorldFlag(world, "flag_base_open", true);
    auto rooms = createAllRooms();
    GameContext context{player, world, rooms};

    const ActionResult result = movePlayer(context, "east");

    assertTrue(result.success, "基地解锁后应允许进入");
    assertTrue(player.getCurrentRoomId() == "room_base", "解锁后应到达基地");
}

void testClimbShortcutDoesNotCostStaminaAtLevelTwo() {
    Player player;
    player.setCurrentRoomId("room_forest");
    setTestPlayerStamina(player, 3);
    setTestPlayerSkill(player, SkillType::Climb, 2);
    WorldState world;
    auto rooms = createAllRooms();
    GameContext context{player, world, rooms};

    const ActionResult result = movePlayer(context, "up");

    assertTrue(result.success, "攀爬二级应可使用树冠捷径");
    assertTrue(player.getCurrentRoomId() == "room_river", "捷径应到达清泉河谷");
    assertTrue(player.getStamina() == 3, "攀爬二级捷径不应消耗体力");
}

void testClimbShortcutCostsStaminaBelowLevelTwo() {
    Player player;
    player.setCurrentRoomId("room_forest");
    setTestPlayerStamina(player, 3);
    setTestPlayerSkill(player, SkillType::Climb, 1);
    WorldState world;
    auto rooms = createAllRooms();
    GameContext context{player, world, rooms};

    const ActionResult result = movePlayer(context, "up");

    assertTrue(result.success, "攀爬一级仍可通过普通方式前往河谷");
    assertTrue(player.getStamina() == 2, "攀爬二级前的捷径应消耗一点体力");
}

int runRoomTests() {
    testMapContainsFiveCanonicalRooms();
    testCaveContainsChipBeforeBaseInfiltration();
    testLookDescribesCurrentRoomAndExits();
    testHelpListsCoreCommands();
    testValidMoveChangesRoomAndConsumesStamina();
    testInvalidMoveLeavesPlayerStateUnchanged();
    testMoveFailsWhenStaminaIsEmpty();
    testBaseEntranceRequiresOpenFlag();
    testOpenBaseFlagAllowsEntry();
    testClimbShortcutDoesNotCostStaminaAtLevelTwo();
    testClimbShortcutCostsStaminaBelowLevelTwo();
    return 0;
}
