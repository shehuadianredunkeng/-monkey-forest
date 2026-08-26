#include "CombatSystem.h"
#include "Item.h"
#include "NPCSystem.h"
#include "Player.h"
#include "Room.h"
#include "WorldState.h"

#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace {
struct FakeWorldData {
    int stage = 1;
    int turns = 0;
    std::map<ResourceType, int> resources;
    std::set<std::string> flags;
};
std::map<const WorldState*, FakeWorldData> worlds;

void expect(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
}

int WorldState::getStage() const { return worlds[this].stage; }
void WorldState::setStage(int stage) { worlds[this].stage = stage; }
int WorldState::getTurnCount() const { return worlds[this].turns; }
void WorldState::consumeTurn() { ++worlds[this].turns; }
void WorldState::resetTurnCount() { worlds[this].turns = 0; }
int WorldState::getResource(ResourceType type) const { return worlds[this].resources[type]; }
void WorldState::changeResource(ResourceType type, int delta) { worlds[this].resources[type] += delta; }
bool WorldState::hasFlag(const std::string& flag) const { return worlds[this].flags.count(flag) != 0; }
void WorldState::setFlag(const std::string& flag) { worlds[this].flags.insert(flag); }
void WorldState::removeFlag(const std::string& flag) { worlds[this].flags.erase(flag); }

void testNpcTasksUsePlayerAndWorldInterfaces() {
    Player player;
    WorldState world;
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    NPCSystem npcs;
    npcs.initializeNPCs();

    expect(player.addItem(Item("item_rope", "藤索", true, 1)), "rope setup failed");
    const ActionResult scout = npcs.completeNPCQuest("npc_scout", ctx);
    expect(scout.success, "scout quest should accept rope");
    expect(world.hasFlag("flag_scout_help"), "scout help flag missing");
    expect(player.hasItem("item_rope"), "important rope should remain in inventory");

    player.changeHealth(-30);
    expect(player.addItem(Item("item_herb", "草药")), "herb setup failed");
    const ActionResult healer = npcs.completeNPCQuest("npc_healer", ctx);
    expect(healer.success && player.getHealth() == 95, "healer quest result mismatch");
    expect(!player.hasItem("item_herb"), "ordinary herb should be consumed");

    world.setFlag("flag_child_found");
    expect(npcs.completeNPCQuest("npc_child", ctx).success,
           "child rescue should complete after event flag");
}

void testRobotHackAndHertzArmor() {
    Player player;
    WorldState world;
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    CombatSystem combat;
    combat.initializeEnemies();

    player.changeWisdom(2);
    expect(combat.startBattle("enemy_robot", ctx).success, "robot battle should start");
    while (combat.isInBattle())
        expect(combat.performBattleAction("hack", "", ctx).success,
               "wisdom-three robot hack should work");
    expect(world.hasFlag("flag_robot_defeated"), "robot victory flag missing");

    player.changeHealth(100);
    player.changeStrength(4);
    player.changeSkillLevel(SkillType::Combat, 2);
    player.changeWisdom(2);
    world.setFlag("flag_complete_log");
    expect(combat.startBattle("enemy_hertz", ctx).success, "Hertz battle should start");
    expect(combat.performBattleAction("analyze", "", ctx).success,
           "log and wisdom should disable Hertz armor");
    while (combat.isInBattle())
        expect(combat.performBattleAction("attack", "", ctx).success,
               "attack should finish Hertz battle");
    expect(world.hasFlag("flag_hertz_defeated"), "Hertz victory flag missing");
}

void testNpcPlacementMatchesQuestFlow() {
    const auto rooms = createAllRooms();
    expect(rooms.at("room_base").getNPCIds().front() == "npc_hertz",
           "Hertz must be visible in base");
    expect(rooms.at("room_river").getNPCIds().front() == "npc_child",
           "child rescue NPC must be in river");
}

int main() {
    try {
        testNpcTasksUsePlayerAndWorldInterfaces();
        testRobotHackAndHertzArmor();
        testNpcPlacementMatchesQuestFlow();
    } catch (const std::exception& error) {
        std::cerr << "member3_npc_combat_test failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
