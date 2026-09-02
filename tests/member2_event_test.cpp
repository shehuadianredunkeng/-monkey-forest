#include "CombatSystem.h"
#include "EventFactory.h"
#include "EventSystem.h"
#include "Item.h"
#include "Player.h"
#include "Room.h"
#include "WorldState.h"

#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

void resetWorld(WorldState& world) {
    world = WorldState{};
}

namespace {

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

namespace {

void testAllElevenEventsCanTrigger() {
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

void testEveryChoiceShowsItsRequirements() {
    const auto events = createAllEvents();
    expect(events.size() == 11,
           "member 2 should provide eight main and three random events");
    expect(events.find("event_injured_child") == events.end(),
           "child rescue must be owned by member 3 NPC dialogue only");

    for (const auto& [id, event] : events) {
        for (const std::string& choice : event.choices) {
            expect(choice.find("需要") != std::string::npos ||
                       choice.find("无属性要求") != std::string::npos ||
                       choice.find("无智慧门槛") != std::string::npos,
                   "choice requirement is not transparent: " + id);
        }
    }
}

void testRoomEntryAutoTriggerAndChineseObjective() {
    Player player;
    WorldState world;
    resetWorld(world);
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    EventSystem events;
    events.initializeEvents();

    const ActionResult wrongRoom = events.triggerAvailableMainEvent(ctx);
    expect(!wrongRoom.success && wrongRoom.message.empty(),
           "main event must not trigger in the wrong room");

    const std::string beforeMove = events.getCurrentObjective(ctx);
    expect(beforeMove.find("树冠试炼") != std::string::npos &&
               beforeMove.find("果实森林") != std::string::npos,
           "objective should show the Chinese event and room names");
    expect(beforeMove.find("event_") == std::string::npos &&
               beforeMove.find("room_") == std::string::npos,
           "objective must hide internal identifiers");

    player.setCurrentRoomId("room_forest");
    const std::string atForest = events.getCurrentObjective(ctx);
    expect(atForest.find("随机（random）") != std::string::npos,
           "eligible random events should have a visible entry hint");

    const ActionResult prompt = events.triggerAvailableMainEvent(ctx);
    expect(prompt.success &&
               prompt.message.find("【事件】树冠试炼") != std::string::npos,
           "room entry should automatically show the available main event");
    expect(events.triggerAvailableMainEvent(ctx).success,
           "re-entering should redisplay rather than duplicate a pending event");
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
    const ActionResult prompt = events.triggerEvent("event_tree_trial", ctx);
    expect(prompt.success, "tree trial should trigger");
    expect(prompt.message.find("【事件】树冠试炼") != std::string::npos &&
               prompt.message.find("需要体力≥10") != std::string::npos,
           "main prompt should use Chinese and show option requirements");
    expect(prompt.message.find("event_tree_trial") == std::string::npos &&
               prompt.message.find("flag_") == std::string::npos,
           "main prompt must hide internal identifiers");
    expect(!events.canTriggerEvent("event_tree_trial", ctx),
           "pending event must not auto-trigger a second time");
    expect(events.triggerEvent("event_tree_trial", ctx).success,
           "investigate should redisplay a pending event");
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

void testWisdomRouteSurvivesMissingTreeReward() {
    Player player;
    WorldState world;
    resetWorld(world);
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    EventSystem events;
    events.initializeEvents();

    expect(player.getWisdom() == 1, "wisdom should start at one");

    world.setStage(3);
    world.setFlag("flag_event_winter_shortage_done");
    player.setCurrentRoomId("room_river");
    expect(events.triggerEvent("event_glowing_river", ctx).success,
           "river research should trigger without tree wisdom");
    expect(events.chooseEventOption("", 2, ctx).success,
           "river research should resolve");
    expect(player.getWisdom() == 2,
           "river research should grant the first later wisdom point");

    player.setCurrentRoomId("room_cave");
    expect(events.triggerEvent("event_echo_tracking", ctx).success,
           "echo research should trigger");
    expect(events.chooseEventOption("", 1, ctx).success,
           "echo research should resolve");
    expect(player.getWisdom() == 3,
           "echo research should grant another wisdom point");

    world.setStage(4);
    player.setCurrentRoomId("room_river");
    expect(events.triggerEvent("event_drought_choice", ctx).success,
           "chip research should trigger");
    expect(events.chooseEventOption("", 2, ctx).success,
           "chip research should resolve");
    expect(player.getWisdom() == 4,
           "three later story nodes should unlock the hack threshold");

    const int wisdomAfterCompletion = player.getWisdom();
    expect(!events.triggerEvent("event_drought_choice", ctx).success,
           "completed research must not repeat");
    expect(player.getWisdom() == wisdomAfterCompletion,
           "repeated investigation must not grant wisdom again");
}

void testDroneAndChipResearchShareOneWisdomReward() {
    Player player;
    WorldState world;
    resetWorld(world);
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    EventSystem events;
    events.initializeEvents();

    world.setStage(2);
    player.setCurrentRoomId("room_river");
    const ActionResult dronePrompt =
        events.triggerEvent("event_drone_crash", ctx);
    expect(dronePrompt.success &&
               dronePrompt.message.find("无智慧门槛") != std::string::npos,
           "drone research should be available at starting wisdom");
    expect(events.chooseEventOption("", 1, ctx).success,
           "drone research should resolve");
    expect(player.getWisdom() == 2,
           "drone research should grant wisdom at starting value");

    world.setStage(4);
    player.setCurrentRoomId("room_river");
    expect(events.triggerEvent("event_drought_choice", ctx).success,
           "later chip research should still prepare the route");
    expect(events.chooseEventOption("", 2, ctx).success,
           "later chip research should resolve");
    expect(player.getWisdom() == 2,
           "drone and chip research must not grant the shared reward twice");
}

void testBaseLogCanProvideFinalWisdomPoint() {
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
    player.changeWisdom(2);
    expect(player.addItem(Item("item_chip", "星猿晶片", true, 1)),
           "chip setup failed");
    expect(events.triggerEvent("event_base_infiltration", ctx).success,
           "base log research should trigger");
    expect(events.chooseEventOption("", 2, ctx).success,
           "base log research should resolve");
    expect(player.getWisdom() == 4,
           "base log research should supply the final hack wisdom point");
}

void testAllRandomEventsCanBeCompleted() {
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
    const ActionResult firstResult = events.chooseEventOption("", 1, ctx);
    expect(firstResult.success, "first random event should finish");
    expect(firstResult.message.find("指引（guide）") != std::string::npos,
           "random event result should explain the next step");

    expect(events.triggerEvent("event_wildfire", ctx).success,
           "second random event in the same room should still trigger");
    expect(events.chooseEventOption("", 3, ctx).success,
           "second random event should finish");

    player.setCurrentRoomId("room_river");
    expect(events.triggerEvent("event_drone_crash", ctx).success,
           "all remaining random events must stay available");
    expect(events.chooseEventOption("", 2, ctx).success,
           "third random event should finish");

    expect(world.hasFlag("flag_event_hidden_orchard_done") &&
               world.hasFlag("flag_event_wildfire_done") &&
               world.hasFlag("flag_event_drone_crash_done"),
           "each random event needs a persistent completion flag");
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
    const ActionResult resumed = events.resumePendingEventAfterBattle(ctx);
    expect(resumed.success && resumed.stageCompleted,
           "event should automatically resume after combat victory");
    expect(!events.resumePendingEventAfterBattle(ctx).success,
           "completed battle event must not resume twice");
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
    world.setFlag("flag_route_resist_ready");
    player.setCurrentRoomId("room_tree");
    player.changeReputation(60);
    player.changeSkillLevel(SkillType::Combat, 1);
    expect(events.triggerEvent("event_final_choice", ctx).success,
           "final choice should trigger");
    expectPlayerFacingStory(events.chooseEventOption("", 1, ctx), "Hertz");
}

void testAllThreeEndingRoutesAreReachableAndPrepared() {
    {
        Player player;
        WorldState world;
        resetWorld(world);
        auto rooms = createAllRooms();
        GameContext ctx{player, world, rooms};
        EventSystem events;
        events.initializeEvents();

        world.setStage(6);
        world.setFlag("flag_complete_log");
        world.setFlag("flag_route_resist_ready");
        player.setCurrentRoomId("room_tree");
        player.changeReputation(60);
        player.changeSkillLevel(SkillType::Combat, 1);
        expect(events.triggerEvent("event_final_choice", ctx).success,
               "resist final choice should trigger");
        expect(events.chooseEventOption("", 1, ctx).success,
               "resist route should enter the Hertz battle bridge");
        world.setFlag("flag_hertz_defeated");
        const ActionResult ending = events.resumePendingEventAfterBattle(ctx);
        expect(ending.success && world.hasFlag("flag_choice_resist"),
               "prepared resist route should reach its ending");
    }

    {
        Player player;
        WorldState world;
        resetWorld(world);
        auto rooms = createAllRooms();
        GameContext ctx{player, world, rooms};
        EventSystem events;
        events.initializeEvents();

        world.setStage(6);
        world.setFlag("flag_complete_log");
        world.setFlag("flag_route_hack_ready");
        player.setCurrentRoomId("room_tree");
        player.changeWisdom(3);
        expect(events.triggerEvent("event_final_choice", ctx).success,
               "hack final choice should trigger");
        const ActionResult ending = events.chooseEventOption("", 2, ctx);
        expect(ending.success && world.hasFlag("flag_choice_hack"),
               "prepared hack route should reach its ending");
    }

    {
        Player player;
        WorldState world;
        resetWorld(world);
        auto rooms = createAllRooms();
        GameContext ctx{player, world, rooms};
        EventSystem events;
        events.initializeEvents();

        world.setStage(6);
        world.setFlag("flag_complete_log");
        world.setFlag("flag_new_home_found");
        world.setResource(ResourceType::MigrationSupply, 8);
        player.setCurrentRoomId("room_tree");
        player.changeReputation(60);
        player.changeWisdom(3);
        player.changeSkillLevel(SkillType::Combat, 1);
        player.changeSkillLevel(SkillType::Leadership, 1);
        expect(events.triggerEvent("event_final_choice", ctx).success,
               "unprepared final choice should still display its options");
        expect(!events.chooseEventOption("", 1, ctx).success,
               "high numbers alone must not bypass resist preparation");
        expect(!events.chooseEventOption("", 2, ctx).success,
               "high numbers alone must not bypass hack preparation");
        expect(!events.chooseEventOption("", 3, ctx).success,
               "high numbers alone must not bypass migration preparation");
    }
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
    EventSystem loaded;
    loaded.initializeEvents();

    world.setStage(1);
    player.setCurrentRoomId("room_forest");
    expect(loaded.triggerEvent("event_tree_trial", ctx).success,
           "old in-memory active event setup failed");

    resetWorld(world);
    world.setStage(2);
    player.setCurrentRoomId("room_forest");
    world.setFlag("flag_pending_event_hidden_orchard");

    expect(loaded.chooseEventOption("", 1, ctx).success,
           "saved pending event must override stale in-memory event after load");
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
    for (int stage = 1; stage <= 6; ++stage) {
        const std::string intro = loaded.getStageIntroduction(stage);
        expect(intro.find("event_") == std::string::npos &&
                   intro.find("flag_") == std::string::npos,
               "stage introduction must not expose internal identifiers");
    }
}

}  // namespace

int main() {
    try {
        testAllElevenEventsCanTrigger();
        testEveryChoiceShowsItsRequirements();
        testRoomEntryAutoTriggerAndChineseObjective();
        testMainEventAndDuplicateProtection();
        testStoryPathGrantsRealChipItem();
        testWisdomRouteSurvivesMissingTreeReward();
        testDroneAndChipResearchShareOneWisdomReward();
        testBaseLogCanProvideFinalWisdomPoint();
        testAllRandomEventsCanBeCompleted();
        testRealCombatBridge();
        testAllBattlePromptsArePlayerFacing();
        testMigrationRouteIsReachable();
        testAllThreeEndingRoutesAreReachableAndPrepared();
        testPendingRecoveryAndStoryText();
    } catch (const std::exception& error) {
        std::cerr << "member2_event_test failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "member2_event_test passed\n";
    return 0;
}
