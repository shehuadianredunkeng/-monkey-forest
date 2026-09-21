#include "Room.h"
#include "TestFramework.h"
#include "TestSupport.h"
#include "WorldState.h"

#include <algorithm>

using namespace std;

void testMapContainsFiveCanonicalRooms() {
    const auto rooms = createAllRooms();

    expect(rooms.size() == 5, "地图应包含五个房间");
    expect(rooms.count("room_tree") == 1, "缺少猴王树");
    expect(rooms.count("room_forest") == 1, "缺少果实森林");
    expect(rooms.count("room_river") == 1, "缺少清泉河谷");
    expect(rooms.count("room_cave") == 1, "缺少回声山洞");
    expect(rooms.count("room_base") == 1, "缺少废弃实验基地");
    expect(rooms.at("room_tree").getExits().count("east") == 1,
               "猴王树应有通往森林的东侧出口");
}

void testCaveContainsChipBeforeBaseInfiltration() {
    const auto rooms = createAllRooms();
    const auto& caveItems = rooms.at("room_cave").getItemIds();
    const auto& baseItems = rooms.at("room_base").getItemIds();

    expect(find(caveItems.begin(), caveItems.end(), "item_chip") != caveItems.end(),
               "回声山洞应提供进入基地所需的晶片");
    expect(find(baseItems.begin(), baseItems.end(), "item_chip") == baseItems.end(),
               "基地内部不应放置用于开启自身的晶片");
}

void testLookDescribesCurrentRoomAndExits() {
    Player player;
    player.setCurrentRoomId("room_tree");
    WorldState world;
    auto rooms = createAllRooms();
    GameContext context{player, world, rooms};

    const string scene = lookAround(context);

    expect(scene.find("猴王树") != string::npos, "观察应显示当前房间名称");
    expect(scene.find("森林") != string::npos, "观察应提示可到达的森林");
    expect(scene.find("出口") != string::npos, "观察应列出出口");
}

void testHelpListsCoreCommands() {
    const string help = getCommandHelp();

    expect(help.find("look") != string::npos, "帮助应包含 look 命令");
    expect(help.find("go") != string::npos, "帮助应包含 go 命令");
    expect(help.find("help") != string::npos, "帮助应包含 help 命令");
}

void testValidMoveChangesRoomAndConsumesStamina() {
    Player player;
    player.setCurrentRoomId("room_tree");
    setTestPlayerStamina(player, 3);
    WorldState world;
    auto rooms = createAllRooms();
    GameContext context{player, world, rooms};

    const ActionResult result = movePlayer(context, "east");

    expect(result.success, "合法出口应允许移动");
    expect(result.turnConsumed, "成功移动应请求主循环消耗回合");
    expect(player.getCurrentRoomId() == "room_forest", "移动后应位于目标房间");
    expect(player.getStamina() == 0, "切换地图应消耗三点体力");
}

void testInvalidMoveLeavesPlayerStateUnchanged() {
    Player player;
    player.setCurrentRoomId("room_tree");
    setTestPlayerStamina(player, 3);
    WorldState world;
    auto rooms = createAllRooms();
    GameContext context{player, world, rooms};

    const ActionResult result = movePlayer(context, "north");

    expect(!result.success, "不存在的出口应失败");
    expect(!result.turnConsumed, "失败移动不应消耗回合");
    expect(player.getCurrentRoomId() == "room_tree", "失败移动不能改变位置");
    expect(player.getStamina() == 3, "失败移动不能消耗体力");
}

void testMoveFailsWhenStaminaIsBelowTransitionCost() {
    Player player;
    player.setCurrentRoomId("room_tree");
    setTestPlayerStamina(player, 2);
    WorldState world;
    auto rooms = createAllRooms();
    GameContext context{player, world, rooms};

    const ActionResult result = movePlayer(context, "east");

    expect(!result.success, "体力不足三点时不应切换地图");
    expect(player.getCurrentRoomId() == "room_tree", "体力不足时应保留原位置");
    expect(player.getStamina() == 2, "失败移动不能消耗体力");
}

void testBaseEntranceRequiresOpenFlag() {
    Player player;
    player.setCurrentRoomId("room_river");
    setTestPlayerStamina(player, 3);
    WorldState world;
    auto rooms = createAllRooms();
    GameContext context{player, world, rooms};

    const ActionResult result = movePlayer(context, "east");

    expect(!result.success, "基地未解锁时不应允许进入");
    expect(player.getCurrentRoomId() == "room_river", "门禁拦截后应保留原位置");
    expect(player.getStamina() == 3, "门禁拦截不应消耗体力");
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

    expect(result.success, "基地解锁后应允许进入");
    expect(player.getCurrentRoomId() == "room_base", "解锁后应到达基地");
}

void testTreeShortcutUsesStrengthAndConsumesTransitionStamina() {
    Player player;
    player.setCurrentRoomId("room_forest");
    setTestPlayerStamina(player, 3);
    setTestPlayerStrength(player, 2);
    WorldState world;
    auto rooms = createAllRooms();
    GameContext context{player, world, rooms};

    const ActionResult result = movePlayer(context, "up");

    expect(result.success, "力量二点应可使用树冠捷径");
    expect(player.getCurrentRoomId() == "room_river", "捷径应到达清泉河谷");
    expect(player.getStamina() == 0, "树冠捷径也应按切换地图消耗三点体力");
}

void testTreeShortcutRejectsLowStrength() {
    Player player;
    player.setCurrentRoomId("room_forest");
    setTestPlayerStamina(player, 3);
    setTestPlayerStrength(player, 1);
    WorldState world;
    auto rooms = createAllRooms();
    GameContext context{player, world, rooms};

    const ActionResult result = movePlayer(context, "up");

    expect(!result.success, "力量不足时不能使用树冠捷径");
    expect(player.getStamina() == 3, "力量不足的失败尝试不能消耗体力");
}

void testRescuedChildMovesFromRiverToKingTree() {
    Player player;
    WorldState world;
    auto rooms = createAllRooms();
    GameContext context{player, world, rooms};

    const auto beforeRiver = rooms.at("room_river").getVisibleNPCIds(context);
    const auto beforeTree = rooms.at("room_tree").getVisibleNPCIds(context);
    expect(find(beforeRiver.begin(), beforeRiver.end(), "npc_child") != beforeRiver.end(),
               "救援前豆豆应在清泉河谷");
    expect(find(beforeTree.begin(), beforeTree.end(), "npc_child") == beforeTree.end(),
               "救援前猴王树不应显示豆豆");

    setTestWorldFlag(world, "flag_child_rescued", true);
    const auto afterRiver = rooms.at("room_river").getVisibleNPCIds(context);
    const auto afterTree = rooms.at("room_tree").getVisibleNPCIds(context);
    expect(find(afterRiver.begin(), afterRiver.end(), "npc_child") == afterRiver.end(),
               "救援后豆豆应离开清泉河谷");
    expect(find(afterTree.begin(), afterTree.end(), "npc_child") != afterTree.end(),
               "救援后豆豆应刷新在猴王树");
}

int runRoomTests() {
    testMapContainsFiveCanonicalRooms();
    testCaveContainsChipBeforeBaseInfiltration();
    testLookDescribesCurrentRoomAndExits();
    testHelpListsCoreCommands();
    testValidMoveChangesRoomAndConsumesStamina();
    testInvalidMoveLeavesPlayerStateUnchanged();
    testMoveFailsWhenStaminaIsBelowTransitionCost();
    testBaseEntranceRequiresOpenFlag();
    testOpenBaseFlagAllowsEntry();
    testTreeShortcutUsesStrengthAndConsumesTransitionStamina();
    testTreeShortcutRejectsLowStrength();
    testRescuedChildMovesFromRiverToKingTree();
    return 0;
}
