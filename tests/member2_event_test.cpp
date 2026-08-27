#include "CombatSystem.h"
#include "EventSystem.h"
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

void resetWorld(WorldState& world) {
    worlds[&world] = FakeWorldData{};
}

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void expectPlayerFacingStory(const ActionResult& result,
                             const std::string& label) {
    expect(result.success, label + " battle bridge should succeed");
    expect(result.message.find("startBattle") == std::string::npos &&
               result.message.find("enemy_") == std::string::npos &&
               result.message.find("choose 1") == std::string::npos,
           label + " story must not expose integration code");
}

}  // namespace

int WorldState::getStage() const { return worlds[this].stage; }
void WorldState::setStage(int stage) { worlds[this].stage = stage; }
int WorldState::getTurnCount() const { return worlds[this].turns; }
void WorldState::consumeTurn() { ++worlds[this].turns; }
void WorldState::resetTurnCount() { worlds[this].turns = 0; }
int WorldState::getResource(ResourceType type) const {
    return worlds[this].resources[type];
}
void WorldState::changeResource(ResourceType type, int delta) {
    worlds[this].resources[type] += delta;
}
bool WorldState::hasFlag(const std::string& flag) const {
    return worlds[this].flags.count(flag) != 0;
}
void WorldState::setFlag(const std::string& flag) {
    worlds[this].flags.insert(flag);
}
void WorldState::removeFlag(const std::string& flag) {
    worlds[this].flags.erase(flag);
}

namespace {

void testAllTwelveEventsCanTrigger() {
    struct TriggerCase {
        std::string id;
        int stage;
        std::string room;
        std::set<std::string> flags;
    };

    const TriggerCase cases[] = {
        {"event_tree_trial", 1, "room_forest", {}},
        {"event_winter_shortage", 2, "room_tree",
         {"flag_event_tree_trial_done"}},
        {"event_glowing_river", 3, "room_river",
         {"flag_event_winter_shortage_done"}},
        {"event_echo_tracking", 3, "room_cave", {"flag_water_fixed"}},
        {"event_drought_choice", 4, "room_river", {"flag_chip_found"}},
        {"event_group_dispute", 4, "room_tree",
         {"flag_event_drought_choice_done"}},
        {"event_base_infiltration", 5, "room_base",
         {"flag_base_open", "flag_chip_found"}},
        {"event_final_choice", 6, "room_tree", {"flag_complete_log"}},
        {"event_wildfire", 2, "room_forest", {}},
        {"event_injured_child", 2, "room_river", {}},
        {"event_hidden_orchard", 2, "room_forest", {}},
        {"event_drone_crash", 2, "room_river", {}}
    };

    for (const TriggerCase& testCase : cases) {
        Player player;
        WorldState world;
        resetWorld(world);
        auto rooms = createAllRooms();
        GameContext ctx{player, world, rooms};
        EventSystem events;
        events.initializeEvents();

        world.setStage(testCase.stage);
        player.setCurrentRoomId(testCase.room);
        for (const std::string& flag : testCase.flags) {
            world.setFlag(flag);
        }
        expect(events.canTriggerEvent(testCase.id, ctx),
               "event should be triggerable: " + testCase.id);
    }
}

void testMainEventAndDuplicateProtection() {
    Player player;
    WorldState world;
    resetWorld(world);
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    EventSystem events;
    events.initializeEvents();

    player.setCurrentRoomId("room_forest");
    expect(events.triggerEvent("event_tree_trial", ctx).success,
           "tree trial should trigger");
    expect(!events.chooseEventOption("", 9, ctx).success,
           "invalid option should fail");
    const ActionResult result = events.chooseEventOption("", 3, ctx);
    expect(result.success && result.turnConsumed && result.stageCompleted,
           "tree trial safe route should finish stage");
    expect(world.hasFlag("flag_event_tree_trial_done"),
           "completion flag missing");
    expect(!events.canTriggerEvent("event_tree_trial", ctx),
           "completed event must not repeat");
}

void testStoryPathGrantsRealChipItem() {
    Player player;
    WorldState world;
    resetWorld(world);
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    EventSystem events;
    events.initializeEvents();

    world.setStage(3);
    world.setFlag("flag_event_winter_shortage_done");
    player.setCurrentRoomId("room_river");
    player.changeWisdom(1);
    expect(events.triggerEvent("event_glowing_river", ctx).success,
           "river event should trigger");
    expect(events.chooseEventOption("", 2, ctx).success,
           "river wisdom route should resolve");

    player.setCurrentRoomId("room_cave");
    expect(events.triggerEvent("event_echo_tracking", ctx).success,
           "echo event should trigger");
    expect(events.chooseEventOption("", 1, ctx).success,
           "echo route should resolve");
    expect(world.hasFlag("flag_chip_found"), "chip flag missing");
    expect(player.hasItem("item_chip"), "real chip item missing from inventory");
}

void testChildEventConnectsToNpcQuest() {
    Player player;
    WorldState world;
    resetWorld(world);
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    EventSystem events;
    NPCSystem npcs;
    events.initializeEvents();
    npcs.initializeNPCs();

    world.setStage(2);
    player.setCurrentRoomId("room_river");
    expect(player.addItem(Item("item_herb", "草药")),
           "herb setup failed");
    expect(events.triggerEvent("event_injured_child", ctx).success,
           "child event should trigger");
    expect(events.chooseEventOption("", 1, ctx).success,
           "child event should find child");
    expect(world.hasFlag("flag_child_found"), "child-found flag missing");
    expect(!world.hasFlag("flag_child_rescued"),
           "event must leave final rescue to NPC module");
    expect(npcs.completeNPCQuest("npc_child", ctx).success,
           "NPC quest should accept child-found flag");
    expect(world.hasFlag("flag_child_rescued"), "NPC rescue flag missing");
}

void testOnlyTwoRandomEventsPerGame() {
    Player player;
    WorldState world;
    resetWorld(world);
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    EventSystem events;
    events.initializeEvents();

    world.setStage(2);
    player.setCurrentRoomId("room_forest");
    expect(events.triggerEvent("event_hidden_orchard", ctx).success,
           "first random event should trigger");
    expect(events.chooseEventOption("", 1, ctx).success,
           "first random event should finish");

    player.setCurrentRoomId("room_river");
    expect(events.triggerEvent("event_drone_crash", ctx).success,
           "second random event should trigger");
    expect(events.chooseEventOption("", 2, ctx).success,
           "second random event should finish");

    player.setCurrentRoomId("room_forest");
    expect(!events.canTriggerEvent("event_wildfire", ctx),
           "third random event must be blocked");
}

void testRealCombatBridge() {
    Player player;
    WorldState world;
    resetWorld(world);
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    EventSystem events;
    CombatSystem combat;
    events.initializeEvents();
    combat.initializeEnemies();

    player.setCurrentRoomId("room_forest");
    expect(events.triggerEvent("event_tree_trial", ctx).success,
           "tree trial should trigger");
    const ActionResult bridge = events.chooseEventOption("", 1, ctx);
    expect(bridge.success && !bridge.turnConsumed,
           "battle bridge should not consume event turn");
    expectPlayerFacingStory(bridge, "bees");
    expect(world.hasFlag("flag_pending_battle_bees"),
           "pending battle flag missing");

    expect(combat.startBattle("enemy_bees", ctx).success,
           "bees battle should start");
    while (combat.isInBattle()) {
        expect(combat.performBattleAction("attack", "", ctx).success,
               "bees battle action failed");
    }
    expect(world.hasFlag("flag_bees_defeated"),
           "combat victory flag missing");
    expect(events.chooseEventOption("", 1, ctx).success,
           "event should finish after combat victory");
}

void testAllBattlePromptsArePlayerFacing() {
    Player player;
    WorldState world;
    resetWorld(world);
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    EventSystem events;
    events.initializeEvents();

    world.setStage(5);
    world.setFlag("flag_base_open");
    world.setFlag("flag_chip_found");
    player.setCurrentRoomId("room_base");
    expect(events.triggerEvent("event_base_infiltration", ctx).success,
           "base infiltration should trigger");
    expectPlayerFacingStory(events.chooseEventOption("", 1, ctx), "robot");

    resetWorld(world);
    world.setStage(6);
    world.setFlag("flag_complete_log");
    player.setCurrentRoomId("room_tree");
    player.changeReputation(60);
    player.changeSkillLevel(SkillType::Combat, 1);
    expect(events.triggerEvent("event_final_choice", ctx).success,
           "final choice should trigger");
    expectPlayerFacingStory(events.chooseEventOption("", 1, ctx), "Hertz");
}

void testMigrationRouteIsReachable() {
    Player player;
    WorldState world;
    resetWorld(world);
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    EventSystem events;
    events.initializeEvents();

    world.setStage(4);
    world.setFlag("flag_chip_found");
    player.setCurrentRoomId("room_river");
    expect(events.triggerEvent("event_drought_choice", ctx).success,
           "migration preparation should trigger");
    expect(events.chooseEventOption("", 3, ctx).success,
           "first migration preparation failed");

    player.setCurrentRoomId("room_tree");
    expect(events.triggerEvent("event_group_dispute", ctx).success,
           "group dispute should trigger");
    expect(events.chooseEventOption("", 3, ctx).success,
           "second migration preparation failed");
    expect(world.getResource(ResourceType::MigrationSupply) == 8,
           "migration path must provide required eight supplies");

    world.setStage(6);
    world.setFlag("flag_complete_log");
    player.changeSkillLevel(SkillType::Leadership, 1);
    expect(events.triggerEvent("event_final_choice", ctx).success,
           "final choice should trigger");
    const ActionResult ending = events.chooseEventOption("", 3, ctx);
    expect(ending.success && ending.stageCompleted,
           "migration ending should be reachable");
    expect(world.hasFlag("flag_choice_migrate"),
           "migration ending flag missing");
}

void testPendingRecoveryAndStoryText() {
    Player player;
    WorldState world;
    resetWorld(world);
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    world.setStage(2);
    player.setCurrentRoomId("room_forest");
    world.setFlag("flag_pending_event_hidden_orchard");

    EventSystem loaded;
    loaded.initializeEvents();
    expect(loaded.chooseEventOption("", 1, ctx).success,
           "pending event should recover after load");
    expect(loaded.getStageIntroduction(1).find("第一年") != std::string::npos,
           "stage introduction missing");
    expect(loaded.getEndingText("ending_resist").find("青木英雄") !=
               std::string::npos,
           "resist ending missing");
    expect(loaded.getEndingText("ending_hack").find("无声胜利") !=
               std::string::npos,
           "hack ending missing");
    expect(loaded.getEndingText("ending_migrate").find("向南的新生") !=
               std::string::npos,
           "migration ending missing");
    expect(loaded.getEndingText("ending_fail").find("失落之谷") !=
               std::string::npos,
           "failure ending missing");
}

}  // namespace

int main() {
    try {
        testAllTwelveEventsCanTrigger();
        testMainEventAndDuplicateProtection();
        testStoryPathGrantsRealChipItem();
        testChildEventConnectsToNpcQuest();
        testOnlyTwoRandomEventsPerGame();
        testRealCombatBridge();
        testAllBattlePromptsArePlayerFacing();
        testMigrationRouteIsReachable();
        testPendingRecoveryAndStoryText();
    } catch (const std::exception& error) {
        std::cerr << "member2_event_test failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "member2_event_test passed\n";
    return 0;
}
