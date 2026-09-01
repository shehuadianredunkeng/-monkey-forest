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

    expect(npcs.talkToNPC("闪尾", ctx).success, "Chinese NPC alias should work");
    expect(npcs.talkToNPC("scout", ctx).message.find("1.") != std::string::npos,
           "second scout talk should show choices");
    expect(npcs.chooseNPCDialogue("闪尾", 2, ctx).success,
           "scout dialogue choice should work");
    expect(player.addItem(Item("item_rope", "藤索", true, 1)), "rope setup failed");
    const ActionResult scout = npcs.talkToNPC("闪尾", ctx);
    expect(scout.success, "scout quest should accept rope");
    expect(world.hasFlag("flag_scout_help"), "scout help flag missing");
    expect(world.hasFlag("flag_skill_escape_unlocked"), "escape skill missing");
    expect(world.hasFlag("flag_scout_banana_promise"), "banana promise missing");
    expect(player.hasItem("item_rope"), "important rope should remain in inventory");

    player.changeHealth(-30);
    expect(player.addItem(Item("item_herb", "草药")), "herb setup failed");
    const ActionResult healer = npcs.talkToNPC("叶婆婆", ctx);
    expect(healer.success && player.getHealth() == 95, "healer quest result mismatch");
    expect(!player.hasItem("item_herb"), "ordinary herb should be consumed");

    expect(npcs.talkToNPC("豆豆", ctx).success,
           "talking to child should find the injured child");
    expect(!npcs.chooseNPCDialogue("豆豆", 2, ctx).success,
           "child treatment must require herb");
    expect(world.hasFlag("flag_achievement_no_rice"),
           "missing-herb achievement flag missing");
    expect(player.addItem(Item("item_herb", "草药")), "second herb setup failed");
    expect(npcs.chooseNPCDialogue("豆豆", 2, ctx).success,
           "child rescue should complete through dialogue");
    expect(world.hasFlag("flag_child_rescued"), "child rescue flag missing");

    player.changeReputation(60);
    expect(npcs.talkToNPC("岩背", ctx).success,
           "king support should complete through talk");
    expect(world.hasFlag("flag_king_support"), "king support flag missing");

    const std::string help = getCommandHelp();
    expect(help.find("quest") == std::string::npos &&
               help.find("finish") == std::string::npos,
           "player help must not expose quest or finish commands");
}

void testTheftAndBeeDefenseAchievements() {
    Player player;
    WorldState world;
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};

    CombatSystem bees;
    expect(bees.startBattle("enemy_bees", ctx).success, "bees should start");
    expect(bees.performBattleAction("偷窃", "", ctx).success,
           "Chinese steal command should work");
    expect(player.hasItem("item_honey"), "bees should yield honey");
    expect(!bees.performBattleAction("偷窃", "", ctx).success,
           "theft should be limited to once per battle");
    expect(bees.performBattleAction("防御", "", ctx).success, "first guard failed");
    expect(bees.performBattleAction("防御", "", ctx).success, "second guard failed");
    expect(bees.performBattleAction("防御", "", ctx).success, "third guard failed");
    expect(world.hasFlag("flag_achievement_you_fight_back"),
           "three-guard achievement missing");
    while (bees.isInBattle())
        expect(bees.performBattleAction("攻击", "", ctx).success, "bees cleanup failed");

    player.changeHealth(100);
    CombatSystem robot;
    expect(robot.startBattle("enemy_robot", ctx).success, "robot should start");
    expect(robot.performBattleAction("steal", "", ctx).success, "robot theft failed");
    expect(player.hasItem("item_material_fragment"), "robot material missing");
    world.setFlag("flag_robot_defeated");

    player.changeHealth(100);
    CombatSystem hertz;
    expect(hertz.startBattle("enemy_hertz", ctx).success, "Hertz should start");
    expect(hertz.performBattleAction("偷", "", ctx).success, "Hertz theft failed");
    expect(player.hasItem("item_book"), "Hertz book missing");
    expect(world.hasFlag("flag_achievement_monkey_borrow"),
           "three-theft achievement missing");
}

void testEscapeSkillAndHertzBananaChoice() {
    Player player;
    WorldState world;
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    CombatSystem combat;
    combat.initializeEnemies();

    expect(combat.startBattle("enemy_bees", ctx).success, "bees should start");
    expect(!combat.performBattleAction("逃跑", "", ctx).success,
           "escape must stay locked before scout quest");
    world.setFlag("flag_skill_escape_unlocked");
    const ActionResult escaped = combat.performBattleAction("逃跑", "", ctx);
    expect(escaped.success && escaped.message.find("闪尾从天而降") != std::string::npos,
           "unlocked escape text mismatch");

    world.setFlag("flag_scout_banana_promise");
    expect(combat.startBattle("enemy_hertz", ctx).message.find("香蕉 1/2/3") != std::string::npos,
           "Hertz should offer banana");
    expect(!combat.performBattleAction("攻击", "", ctx).success,
           "normal actions must wait for banana choice");
    player.changeWisdom(2);
    expect(combat.performBattleAction("香蕉", "3", ctx).success,
           "wisdom 3 should reject banana");
}

void testHertzBananaBadEndings() {
    Player player;
    WorldState world;
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    CombatSystem combat;
    world.setFlag("flag_scout_banana_promise");
    combat.initializeEnemies();
    expect(combat.startBattle("enemy_hertz", ctx).success, "Hertz should start");
    expect(combat.performBattleAction("banana", "2", ctx).success,
           "eating first banana should start greed loop");
    const ActionResult ending = combat.performBattleAction("香蕉", "2", ctx);
    expect(!ending.success && ending.stageCompleted,
           "refusing after first banana should produce bad ending");
    expect(world.hasFlag("flag_bad_ending_second_banana"),
           "second banana bad ending flag missing");
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
        expect(combat.performBattleAction("破解", "", ctx).success,
               "Chinese hack command should work");
    expect(world.hasFlag("flag_robot_defeated"), "robot victory flag missing");

    player.changeHealth(100);
    player.changeStrength(4);
    player.changeSkillLevel(SkillType::Combat, 2);
    player.changeWisdom(2);
    world.setFlag("flag_complete_log");
    expect(combat.startBattle("enemy_hertz", ctx).success, "Hertz battle should start");
    expect(combat.performBattleAction("分析", "", ctx).success,
           "Chinese analyze command should disable Hertz armor");
    while (combat.isInBattle())
        expect(combat.performBattleAction("攻击", "", ctx).success,
               "Chinese attack command should finish Hertz battle");
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
        testTheftAndBeeDefenseAchievements();
        testRobotHackAndHertzArmor();
        testEscapeSkillAndHertzBananaChoice();
        testHertzBananaBadEndings();
        testNpcPlacementMatchesQuestFlow();
    } catch (const std::exception& error) {
        std::cerr << "member3_npc_combat_test failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
