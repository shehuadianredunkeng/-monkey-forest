#include "CombatSystem.h"
#include "CollectionSystem.h"
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
    expect(npcs.talkToNPC("闪尾", ctx).message.find("1.") != std::string::npos,
           "second scout talk should show choices");
    expect(npcs.chooseDialogueOption(2, ctx).success,
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
    expect(npcs.talkToNPC("豆豆", ctx).message.find("请直接输入") != std::string::npos,
           "second child talk should open bare-number choices");
    expect(!npcs.chooseDialogueOption(2, ctx).success,
           "child treatment must require herb");
    expect(world.hasFlag("flag_achievement_no_rice"),
           "missing-herb achievement flag missing");
    expect(player.addItem(Item("item_herb", "草药")), "second herb setup failed");
    expect(npcs.chooseDialogueOption(2, ctx).success,
           "child rescue should complete through dialogue");
    expect(world.hasFlag("flag_child_rescued"), "child rescue flag missing");
    expect(player.getCurrentRoomId() == "room_tree",
           "carrying child home should move player to king tree");

    player.changeReputation(60);
    expect(npcs.talkToNPC("岩背", ctx).success,
           "king support should complete through talk");
    expect(world.hasFlag("flag_king_support"), "king support flag missing");

    const std::string help = getCommandHelp();
    expect(help.find("quest") == std::string::npos &&
               help.find("finish") == std::string::npos,
           "player help must not expose quest or finish commands");

    // 3号发布入队状态；地图是否显示该NPC由1号根据此状态处理。
    expect(world.hasFlag("flag_scout_quest_complete") &&
               world.hasFlag("flag_scout_help"),
           "scout quest must publish the map-facing party flags");
    expect(npcs.talkToNPC("scout", ctx).success,
           "English NPC name should remain accepted");
}

void testRepeatedEscapeEndingAndScoutDeparture() {
    Player player;
    WorldState world;
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    CombatSystem combat;
    world.setFlag("flag_skill_escape_unlocked");
    world.setFlag("flag_scout_help");
    expect(player.addItem(Item("item_fruit", "果实")), "fruit setup failed");

    for (int count = 0; count < 4; ++count) {
        expect(combat.startBattle("enemy_bees", ctx).success, "escape battle should start");
        const ActionResult escaped = combat.performBattleAction("逃跑", "", ctx);
        expect(escaped.success, "escape should succeed");
        if (count == 3)
            expect(escaped.message.find("闯荡天涯") != std::string::npos,
                   "fourth escape should trigger invitation");
    }
    const ActionResult refused = combat.chooseEscapeEndingOption(2, ctx);
    expect(refused.success && world.hasFlag("flag_scout_left"),
           "refusing invitation should remove scout");
    expect(!player.hasItem("item_fruit"), "scout should take one fruit");

    Player endingPlayer;
    WorldState endingWorld;
    auto endingRooms = createAllRooms();
    GameContext endingCtx{endingPlayer, endingWorld, endingRooms};
    CombatSystem endingCombat;
    endingWorld.setFlag("flag_skill_escape_unlocked");
    endingWorld.setFlag("flag_scout_help");
    endingWorld.setFlag("flag_escape_used_1");
    endingWorld.setFlag("flag_escape_used_2");
    endingWorld.setFlag("flag_escape_used_3");
    expect(endingCombat.startBattle("enemy_bees", endingCtx).success,
           "hidden-ending battle should start");
    expect(endingCombat.performBattleAction("escape", "", endingCtx).success,
           "hidden-ending escape should work");
    const ActionResult accepted = endingCombat.chooseEscapeEndingOption(1, endingCtx);
    expect(accepted.success && accepted.stageCompleted,
           "accepting invitation should complete hidden ending");
    expect(endingWorld.hasFlag("flag_hidden_ending_together_forever") &&
               endingWorld.hasFlag("flag_achievement_no_monkey_at_tree"),
           "hidden ending and achievement flags missing");
    CollectionSystem collections;
    expect(collections.isEndingUnlocked("ending_together_forever", endingWorld) &&
               collections.isAchievementUnlocked(
                   "achievement_no_monkey_at_tree", endingWorld),
           "hidden ending should enter both collections");
}

void testCollectionSystemSupportsNewAndLegacyEndings() {
    WorldState world;
    CollectionSystem collections;
    expect(collections.endings().size() == 8, "all current endings must be registered");
    expect(collections.unlockEnding("ending_resist", world),
           "registered main ending should unlock");
    expect(collections.unlockedEndingCount(world) == 1,
           "ending collection should count without duplicates");
    expect(collections.unlockEnding("ending_resist", world) &&
               collections.unlockedEndingCount(world) == 1,
           "repeated ending must remain deduplicated");

    collections.registerEnding({"ending_future_test", "未来结局", "测试扩展", true});
    expect(collections.unlockEnding("ending_future_test", world),
           "other members should be able to register a new ending");
    expect(collections.unlockedEndingCount(world) == 2,
           "new registered ending should automatically enter collection count");

    WorldState legacyWorld;
    legacyWorld.setFlag("flag_bad_ending_gluttony");
    collections.syncLegacyFlags(legacyWorld);
    expect(collections.isEndingUnlocked("ending_gluttony", legacyWorld),
           "old ending flags should be imported");
}

void testStoryEventAchievementsUseMember2Flags() {
    WorldState world;
    CollectionSystem collections;

    world.setFlag("flag_event_wildfire_done");
    world.setFlag("flag_event_hidden_orchard_done");
    world.setFlag("flag_event_drone_crash_done");
    world.setFlag("flag_complete_log");
    world.setFlag("flag_route_resist_ready");
    world.setFlag("flag_route_hack_ready");
    world.setFlag("flag_route_migrate_ready");
    collections.syncLegacyFlags(world);

    expect(collections.isAchievementUnlocked("achievement_all_random_events", world),
           "all member-2 random event flags should unlock the encounter achievement");
    expect(collections.isAchievementUnlocked("achievement_pacifist_log", world),
           "a battle-free complete log should unlock the pacifist achievement");
    expect(collections.isAchievementUnlocked("achievement_all_routes_ready", world),
           "all three route-ready flags should unlock the route achievement");

    WorldState foughtWorld;
    foughtWorld.setFlag("flag_complete_log");
    foughtWorld.setFlag("flag_bees_defeated");
    collections.syncLegacyFlags(foughtWorld);
    expect(!collections.isAchievementUnlocked("achievement_pacifist_log", foughtWorld),
           "any victory flag should block the battle-free log achievement");
}

void testYearlyNpcDialogueAndChildReturnFlags() {
    Player player;
    WorldState world;
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    NPCSystem npcs;
    npcs.initializeNPCs();

    std::set<std::string> kingLines;
    std::set<std::string> childLines;
    std::set<std::string> scoutLines;
    world.setFlag("flag_child_rescued");
    world.setFlag("flag_scout_quest_complete");
    world.setFlag("flag_scout_help");
    for (int year = 1; year <= 6; ++year) {
        world.setStage(year);
        kingLines.insert(npcs.talkToNPC("king", ctx).message);
        childLines.insert(npcs.talkToNPC("child", ctx).message);
        scoutLines.insert(npcs.talkToNPC("scout", ctx).message);
    }
    expect(kingLines.size() == 6, "king should have distinct dialogue for all six years");
    expect(childLines.size() == 6, "rescued child should have distinct dialogue for all six years");
    expect(scoutLines.size() == 6, "recruited scout should have distinct dialogue for all six years");

    Player rescuePlayer;
    WorldState rescueWorld;
    auto rescueRooms = createAllRooms();
    GameContext rescueCtx{rescuePlayer, rescueWorld, rescueRooms};
    NPCSystem rescueNpcs;
    rescueNpcs.initializeNPCs();
    rescueNpcs.talkToNPC("豆豆", rescueCtx);
    rescueNpcs.talkToNPC("豆豆", rescueCtx);
    expect(rescuePlayer.addItem(Item("item_herb", "草药")), "rescue herb setup failed");
    expect(rescueNpcs.chooseDialogueOption(2, rescueCtx).success,
           "carrying the child home should succeed");
    expect(rescueWorld.hasFlag("flag_child_saved") &&
               rescueWorld.hasFlag("flag_child_returned") &&
               rescueWorld.hasFlag("flag_child_carried_home"),
           "child rescue must publish the map-compatible return flags");
}

void testBattleVictoryFlagsKeepMember2PendingFlags() {
    struct BattleCase {
        const char* pendingFlag;
        const char* enemyId;
        const char* victoryFlag;
    };
    const BattleCase cases[] = {
        {"flag_pending_battle_bees", "enemy_bees", "flag_bees_defeated"},
        {"flag_pending_battle_robot", "enemy_robot", "flag_robot_defeated"},
        {"flag_pending_battle_hertz", "enemy_hertz", "flag_hertz_defeated"},
    };

    for (const BattleCase& battleCase : cases) {
        Player player;
        WorldState world;
        auto rooms = createAllRooms();
        GameContext ctx{player, world, rooms};
        CombatSystem combat;
        player.changeStrength(20);
        player.changeSkillLevel(SkillType::Combat, 10);
        player.changeWisdom(5);
        player.changeHealth(1000);
        world.setFlag(battleCase.pendingFlag);
        world.setFlag("flag_complete_log");

        expect(combat.startBattle(battleCase.enemyId, ctx).success,
               "story-linked battle should start");
        if (std::string(battleCase.enemyId) == "enemy_hertz")
            expect(combat.performBattleAction("analyze", "", ctx).success,
                   "Hertz armor analysis should succeed");
        while (combat.isInBattle())
            expect(combat.performBattleAction("attack", "", ctx).success,
                   "story-linked battle should finish");

        expect(world.hasFlag(battleCase.victoryFlag),
               "combat must publish the victory flag expected by member 2");
        expect(world.hasFlag(battleCase.pendingFlag),
               "combat must leave the pending flag for EventSystem resume");
    }
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
    Player player;
    WorldState world;
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    NPCSystem npcs;
    npcs.initializeNPCs();
    expect(rooms.at("room_base").getNPCIds().front() == "npc_hertz",
           "Hertz must be visible in base");
    expect(rooms.at("room_river").getNPCIds().front() == "npc_child",
           "child rescue NPC must be in river");
    // NPC显示与离队后的地图隐藏由1号Room模块负责；3号只保证标准ID与
    // 中英文名称均能进入同一套对话逻辑。
    expect(npcs.talkToNPC("scout", ctx).success,
           "member-1 standard scout ID alias should open dialogue");
    expect(npcs.talkToNPC("闪尾", ctx).success,
           "Chinese scout name should open the same dialogue flow");
}

int main() {
    try {
        testNpcTasksUsePlayerAndWorldInterfaces();
        testTheftAndBeeDefenseAchievements();
        testRobotHackAndHertzArmor();
        testEscapeSkillAndHertzBananaChoice();
        testHertzBananaBadEndings();
        testRepeatedEscapeEndingAndScoutDeparture();
        testCollectionSystemSupportsNewAndLegacyEndings();
        testStoryEventAchievementsUseMember2Flags();
        testYearlyNpcDialogueAndChildReturnFlags();
        testBattleVictoryFlagsKeepMember2PendingFlags();
        testNpcPlacementMatchesQuestFlow();
    } catch (const std::exception& error) {
        std::cerr << "member3_npc_combat_test failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
