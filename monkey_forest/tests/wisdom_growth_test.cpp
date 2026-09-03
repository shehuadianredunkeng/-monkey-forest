#include "EndingSystem.h"
#include "EventSystem.h"
#include "Item.h"
#include "Player.h"
#include "ProgressSystem.h"
#include "Room.h"
#include "SaveManager.h"
#include "WorldState.h"

#include <array>
#include <chrono>
#include <climits>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct Game {
    Player player;
    WorldState world;
    std::map<std::string, Room> rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    EventSystem events;
    Game() { events.initializeEvents(); }
};

constexpr std::array<const char*, 5> targets = {
    "树冠", "管线", "碑文", "晶片", "日志"};
constexpr std::array<const char*, 5> flags = {
    "flag_wisdom_tree", "flag_wisdom_river", "flag_wisdom_cave",
    "flag_route_hack_ready", "flag_wisdom_base"};

void chip(Game& game) {
    if (!game.player.hasItem("item_chip")) {
        expect(game.player.addItem(Item("item_chip", "星猿晶片", true)),
               "chip fixture must fit");
    }
    game.world.setFlag("flag_chip_found");
}

ActionResult choice(Game& game, const std::string& event, int option) {
    expect(game.events.triggerEvent(event, game.ctx).success,
           "event must open: " + event);
    return game.events.chooseEventOption("", option, game.ctx);
}

// Stage fixtures isolate reward behavior; wisdom_main_test exercises real progression.
ActionResult earn(Game& game, int node) {
    switch (node) {
    case 0:
        game.world.setStage(1);
        game.player.setCurrentRoomId("room_forest");
        return choice(game, "event_tree_trial", 3);
    case 1:
        game.world.setStage(3);
        game.world.setFlag("flag_event_winter_shortage_done");
        game.player.setCurrentRoomId("room_river");
        return choice(game, "event_glowing_river", 2);
    case 2:
        game.world.setStage(3);
        game.world.setFlag("flag_water_fixed");
        game.player.setCurrentRoomId("room_cave");
        return choice(game, "event_echo_tracking", 1);
    case 3:
        game.world.setStage(4);
        chip(game);
        game.player.setCurrentRoomId("room_river");
        return choice(game, "event_drought_choice", 2);
    default:
        game.world.setStage(5);
        chip(game);
        game.world.setFlag("flag_base_open");
        game.world.setFlag("flag_scout_help");
        game.player.setCurrentRoomId("room_base");
        expect(choice(game, "event_base_infiltration", 3).success,
               "companion entry must obtain logs without wisdom requirement");
        return game.events.triggerEvent("日志", game.ctx);
    }
}

void repeatWithoutEffects(Game& game, const std::string& target) {
    const int wisdom = game.player.getWisdom();
    const int health = game.player.getHealth();
    const int stamina = game.player.getStamina();
    const int reputation = game.player.getReputation();
    const auto oldFlags = game.world.getFlags();
    const int food = game.world.getResource(ResourceType::Food);
    const int water = game.world.getResource(ResourceType::Water);
    const int stage = game.world.getStage();
    const int turns = game.world.getTurnCount();
    const auto count = game.player.getInventory().getItems().size();
    const auto result = game.events.triggerEvent(target, game.ctx);
    expect(result.message.find("已经") != std::string::npos,
           "repeat must explain that the research is already understood");
    expect(result.message.find("【智慧 +1】") == std::string::npos,
           "repeat must not announce a reward");
    expect(!result.turnConsumed && !result.stageCompleted,
           "repeat must not consume a turn or advance the story");
    expect(game.player.getWisdom() == wisdom &&
           game.player.getHealth() == health &&
           game.player.getStamina() == stamina &&
           game.player.getReputation() == reputation,
           "repeat must not change player attributes");
    expect(game.world.getFlags() == oldFlags &&
           game.world.getResource(ResourceType::Food) == food &&
           game.world.getResource(ResourceType::Water) == water &&
           game.world.getStage() == stage && game.world.getTurnCount() == turns,
           "repeat must not replay story side effects");
    expect(game.player.getInventory().getItems().size() == count,
           "repeat must not duplicate items");
}

void testBounds() {
    Player player;
    expect(player.getWisdom() == 1, "initial wisdom must be 1");
    player.changeWisdom(1);
    expect(player.getWisdom() == 2, "one reward must produce wisdom 2");
    player.changeWisdom(100);
    expect(player.getWisdom() == 5, "wisdom upper bound must be 5");
    player.changeWisdom(-100);
    expect(player.getWisdom() == 1, "wisdom lower bound must be 1");
    player.changeWisdom(INT_MAX);
    expect(player.getWisdom() == 5, "extreme positive delta must not overflow");
    player.changeWisdom(INT_MIN);
    expect(player.getWisdom() == 1, "extreme negative delta must not overflow");
}

void testNode(int node) {
    Game game;
    const auto first = earn(game, node);
    expect(first.success, "first research must succeed at wisdom 1");
    expect(game.player.getWisdom() == 2, "first research must grant exactly +1");
    expect(first.message.find("【智慧 +1】") != std::string::npos,
           "successful reward must be visible in Chinese");
    expect(game.world.hasFlag(flags[node]), "reward must enter persistent state");
    for (const auto* internal : {"flag_", "event_", "item_", "npc_"}) {
        expect(first.message.find(internal) == std::string::npos,
               "research text must not expose internal IDs");
    }
    repeatWithoutEffects(game, targets[node]);
    game.events.initializeEvents();
    repeatWithoutEffects(game, targets[node]);
    if (node == 3) expect(game.player.hasItem("item_chip"), "research keeps chip");
}

void testEveryThreeNodes() {
    int combinations = 0;
    for (int first = 0; first < 3; ++first) {
        for (int second = first + 1; second < 4; ++second) {
            for (int third = second + 1; third < 5; ++third) {
                Game game;
                expect(earn(game, first).success, "first selected node");
                expect(earn(game, second).success, "second selected node");
                expect(earn(game, third).success, "third selected node");
                expect(game.player.getWisdom() == 4,
                       "any three of five nodes must reach wisdom 4");
                ++combinations;
            }
        }
    }
    expect(combinations == 10, "exercise all ten three-node combinations");
}

void testAllNodesAndCap() {
    Game all;
    for (int node = 0; node < 5; ++node) {
        expect(earn(all, node).success, "every node must remain completable");
    }
    expect(all.player.getWisdom() == 5, "all five rewards must cap at 5, not 6");
    for (int node = 0; node < 5; ++node) {
        Game capped;
        capped.player.changeWisdom(100);
        const auto result = earn(capped, node);
        expect(result.success && capped.player.getWisdom() == 5,
               "fresh capped node must still complete");
        expect(capped.world.hasFlag(flags[node]), "cap must still consume reward");
        expect(result.message.find("【智慧 +1】") == std::string::npos &&
               result.message.find("智慧+1") == std::string::npos,
               "capped reward must not claim +1");
        repeatWithoutEffects(capped, targets[node]);
    }
}

void testCaveRetry() {
    Game game;
    game.world.setStage(3);
    game.world.setFlag("flag_water_fixed");
    game.player.setCurrentRoomId("room_cave");
    game.player.changeStamina(-100);
    expect(game.events.triggerEvent("event_echo_tracking", game.ctx).success,
           "cave must open before choosing a route");
    expect(!game.events.chooseEventOption("", 9, game.ctx).success,
           "wrong choice must fail safely");
    expect(!game.events.chooseEventOption("", 1, game.ctx).success,
           "insufficient stamina must leave a retry");
    expect(!game.world.hasFlag("flag_event_echo_tracking_done") &&
           !game.world.hasFlag("flag_wisdom_cave"), "failure must not finish cave");
    game.player.changeStamina(30);
    expect(game.events.chooseEventOption("", 1, game.ctx).success,
           "cave must permit retry at wisdom 1");
    expect(game.player.getWisdom() == 2, "successful retry awards cave wisdom");
    repeatWithoutEffects(game, "回声");
}

void testChipPathsShareOneReward() {
    for (bool droneFirst : {false, true}) {
        Game game;
        if (!droneFirst) expect(earn(game, 3).success, "chip study first");
        game.world.setStage(4);
        game.player.setCurrentRoomId("room_river");
        expect(choice(game, "event_drone_crash", 1).success,
               "drone core research must not require wisdom 2");
        if (droneFirst) expect(earn(game, 3).success, "chip study after drone");
        expect(game.player.getWisdom() == 2, "chip and drone are ONE reward");
        expect(game.world.hasFlag("flag_drone_analyzed") &&
               game.world.hasFlag("flag_route_hack_ready"),
               "both story outcomes still complete");
        expect(game.player.hasItem("item_chip"), "neither study consumes chip");
        repeatWithoutEffects(game, "星猿晶片");
    }
}

void testChipResearchDoesNotNeedRandomEvent() {
    Game game;
    game.world.setStage(4);
    game.world.setFlag("flag_event_wildfire_done");
    game.world.setFlag("flag_event_hidden_orchard_done");
    chip(game);
    game.player.setCurrentRoomId("room_river");
    expect(!game.events.canTriggerEvent("event_drone_crash", game.ctx),
           "random event quota fixture must block drone");
    expect(game.events.triggerEvent("晶片", game.ctx).success,
           "held chip research must not depend on random event quota");
    expect(game.player.getWisdom() == 2 && game.player.hasItem("item_chip"),
           "research must reward once while preserving chip");
    expect(!game.world.hasFlag("flag_event_drought_choice_done"),
           "standalone study must not skip a main event");
}

void testMissingChipAndLogsAreRejected() {
    Game game;
    game.world.setStage(4);
    game.world.setFlag("flag_chip_found");
    game.player.setCurrentRoomId("room_river");
    expect(!choice(game, "event_drought_choice", 2).success,
           "chip-found flag alone must not permit research");
    expect(!game.events.triggerEvent("晶片", game.ctx).success,
           "investigation also requires real chip");
    expect(game.player.getWisdom() == 1 &&
           !game.world.hasFlag("flag_route_hack_ready"), "failure gives no reward");
    game.world = WorldState{};
    game.world.setStage(5);
    game.world.setFlag("flag_base_open");
    game.player.setCurrentRoomId("room_base");
    chip(game);
    expect(!game.events.triggerEvent("日志", game.ctx).success,
           "base research must not manufacture complete logs");
    expect(!game.world.hasFlag("flag_complete_log") &&
           !game.world.hasFlag("flag_wisdom_base"), "missing logs stay missing");
}

void testLaterStudyPreservesPendingFinalEvent() {
    Game game;
    game.world.setStage(6);
    game.world.setFlag("flag_complete_log");
    game.world.setFlag("flag_base_open");
    game.world.setFlag("flag_event_glowing_river_done");
    game.world.setFlag("flag_water_fixed");
    game.world.setFlag("flag_event_echo_tracking_done");
    chip(game);
    game.player.setCurrentRoomId("room_tree");
    expect(game.events.triggerEvent("event_final_choice", game.ctx).success,
           "final event must be pending");
    expect(!game.events.chooseEventOption("", 2, game.ctx).success,
           "wisdom 1 must not unlock the ending");
    game.player.setCurrentRoomId("room_river");
    expect(game.events.triggerEvent("管线", game.ctx).success, "late river study");
    game.player.setCurrentRoomId("room_cave");
    expect(game.events.triggerEvent("碑文", game.ctx).success, "late cave study");
    expect(game.events.triggerEvent("晶片", game.ctx).success, "late chip study");
    expect(game.player.getWisdom() == 4, "late research must prevent pending lock");
    expect(game.world.hasFlag("flag_pending_event_final_choice"),
           "follow-up study must preserve the final event");
    game.player.setCurrentRoomId("room_tree");
    expect(game.events.chooseEventOption("", 2, game.ctx).success,
           "original final choice remains usable after research");
    expect(game.world.hasFlag("flag_system_hacked"), "ending still hacks system");
}

class SaveFile {
public:
    SaveFile() : path(std::filesystem::temp_directory_path() /
        ("wisdom_growth_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt")) {}
    ~SaveFile() { std::error_code error; std::filesystem::remove(path, error); }
    std::filesystem::path path;
};

void testSaveLoadEveryNode() {
    for (int node = 0; node < 5; ++node) {
        Game before;
        expect(earn(before, node).success, "award before save");
        SaveFile file;
        SaveManager saves;
        expect(saves.saveGame(file.path.string(), before.ctx), "save reward");
        Game after;
        expect(saves.loadGame(file.path.string(), after.ctx), "load fresh objects");
        expect(after.player.getWisdom() == 2 && after.world.hasFlag(flags[node]),
               "wisdom and reward state must both restore");
        repeatWithoutEffects(after, targets[node]);
        if (node == 3) expect(after.player.hasItem("item_chip"), "loaded chip kept");
    }
}

void testOldSaveMissingFlags() {
    Game old;
    old.world.setStage(6);
    old.world.setFlag("flag_event_glowing_river_done");
    old.world.setFlag("flag_water_fixed");
    old.world.setFlag("flag_water_clue"); // Old low-wisdom branch set this WITHOUT reward.
    old.player.setCurrentRoomId("room_river");
    SaveFile file;
    SaveManager saves;
    expect(saves.saveGame(file.path.string(), old.ctx), "write version-1 old shape");
    Game loaded;
    expect(saves.loadGame(file.path.string(), loaded.ctx), "old save must load");
    expect(!loaded.world.hasFlag("flag_wisdom_river"), "new flag defaults unset");
    expect(loaded.events.triggerEvent("银色管线", loaded.ctx).success,
           "old clue must not incorrectly suppress a missing reward");
    expect(loaded.player.getWisdom() == 2, "old low-wisdom save can study once");
    repeatWithoutEffects(loaded, "管线");
}

void testTreeAlternativeAndClosedEnding() {
    Game game;
    game.player.setCurrentRoomId("room_forest");
    expect(choice(game, "event_tree_trial", 2).stageCompleted,
           "climbing tree must still progress main story");
    expect(game.player.getWisdom() == 1 && !game.world.hasFlag("flag_wisdom_tree"),
           "non-observation must not receive tree reward");
    game.events.triggerEvent("树冠", game.ctx);
    expect(game.player.getWisdom() == 1, "completed trial must not be re-chosen");
    game.world.setStage(6);
    game.world.setFlag("flag_final_choice");
    chip(game);
    expect(!game.events.triggerEvent("晶片", game.ctx).success,
           "study must not mutate an already finalized game");
    expect(game.player.getWisdom() == 1, "finished game must not gain wisdom");
}

void testBaseStealthSharesLogReward() {
    Game game;
    game.world.setStage(5);
    game.world.setFlag("flag_base_open");
    chip(game);
    game.player.setCurrentRoomId("room_base");
    expect(!choice(game, "event_base_infiltration", 2).success,
           "original stealth requirement must remain wisdom 3");
    expect(!game.world.hasFlag("flag_complete_log"), "failed entry yields no logs");
    game.player.changeWisdom(2);
    expect(game.events.chooseEventOption("", 2, game.ctx).success,
           "original stealth works once prerequisites are met");
    expect(game.player.getWisdom() == 4 && game.world.hasFlag("flag_wisdom_base"),
           "stealth log analysis grants the same base reward");
    repeatWithoutEffects(game, "控制台");
    expect(game.player.hasItem("item_chip"), "base study keeps chip");
}

void testDroneSaveGuard() {
    Game before;
    before.world.setStage(2);
    before.player.setCurrentRoomId("room_river");
    expect(choice(before, "event_drone_crash", 1).success, "drone study at wisdom 1");
    SaveFile file;
    SaveManager saves;
    expect(saves.saveGame(file.path.string(), before.ctx), "save drone outcome");
    Game after;
    expect(saves.loadGame(file.path.string(), after.ctx), "reload drone outcome");
    expect(after.world.hasFlag("flag_drone_analyzed") &&
           !after.world.hasFlag("flag_route_hack_ready"), "restore correct outcome");
    repeatWithoutEffects(after, "晶片");
    expect(earn(after, 3).success, "main preparation still progresses after load");
    expect(after.player.getWisdom() == 2, "loaded drone prevents second chip point");
}

void testPrerequisiteFailuresLeaveNoReward() {
    Game game;
    game.world.setStage(3);
    game.world.setFlag("flag_event_winter_shortage_done");
    game.player.setCurrentRoomId("room_tree");
    expect(!game.events.triggerEvent("管线", game.ctx).success,
           "pipe study must require the river location");
    game.player.setCurrentRoomId("room_river");
    expect(game.events.canTriggerEvent("银色管线", game.ctx),
           "Chinese alias availability must match triggering");
    expect(game.events.triggerEvent("银色管线", game.ctx).success,
           "alias must open the original river event");
    expect(game.player.getWisdom() == 1, "opening prompt must not grant wisdom");
    game.player.changeStamina(-100);
    expect(!game.events.chooseEventOption("", 2, game.ctx).success,
           "failed river choice must remain retryable");
    expect(!game.world.hasFlag("flag_wisdom_river"), "failure must not claim reward");
    game.player.changeStamina(20);
    expect(game.events.chooseEventOption("", 2, game.ctx).success,
           "river retry must succeed at wisdom 1");
    expect(game.player.getWisdom() == 2, "retry awards exactly one point");

    Game cave;
    cave.world.setStage(3);
    cave.world.setFlag("flag_water_fixed");
    cave.player.setCurrentRoomId("room_cave");
    for (int i = 0; i < 8; ++i) {
        expect(cave.player.addItem(Item("test_" + std::to_string(i), "测试物品")),
               "fill backpack for failure fixture");
    }
    expect(!choice(cave, "event_echo_tracking", 1).success,
           "full backpack must not discard chip or complete reward");
    expect(cave.player.getWisdom() == 1 && !cave.world.hasFlag("flag_wisdom_cave"),
           "failed chip acquisition cannot award cave wisdom");
    expect(cave.player.removeItem("test_0"), "free one backpack slot");
    expect(cave.events.chooseEventOption("", 1, cave.ctx).success,
           "cave retry after freeing space must work");
    expect(cave.player.getWisdom() == 2 && cave.player.hasItem("item_chip"),
           "successful retry must keep chip and award exactly once");
}

} // namespace

int main() {
    int failures = 0;
    int passed = 0;
    auto run = [&](const std::string& name, auto test) {
        try { test(); ++passed; }
        catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        }
    };
    run("Player bounds", testBounds);
    for (int node = 0; node < 5; ++node) {
        run(targets[node], [node] { testNode(node); });
    }
    run("all ten three-node combinations", testEveryThreeNodes);
    run("all rewards and fresh capped nodes", testAllNodesAndCap);
    run("cave retry", testCaveRetry);
    run("chip/drone shared reward", testChipPathsShareOneReward);
    run("chip independent of random quota", testChipResearchDoesNotNeedRandomEvent);
    run("missing prerequisites", testMissingChipAndLogsAreRejected);
    run("pending final event follow-up", testLaterStudyPreservesPendingFinalEvent);
    run("save/load every node", testSaveLoadEveryNode);
    run("old-save compatibility", testOldSaveMissingFlags);
    run("optional tree and finalized game", testTreeAlternativeAndClosedEnding);
    run("base stealth and follow-up share reward", testBaseStealthSharesLogReward);
    run("drone save/load shared guard", testDroneSaveGuard);
    run("location, stamina and backpack retries", testPrerequisiteFailuresLeaveNoReward);
    std::cout << "wisdom_growth_test: " << passed << " passed, "
              << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
