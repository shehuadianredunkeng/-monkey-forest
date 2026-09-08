#include "EndingSystem.h"
#include "EventSystem.h"
#include "Item.h"
#include "Player.h"
#include "ProgressSystem.h"
#include "Room.h"
#include "SaveManager.h"
#include "StatusView.h"
#include "WorldState.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <string>

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path tempSavePath() {
    auto path = std::filesystem::temp_directory_path() /
                "monkey_forest_member5_save.txt";
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return path;
}

void testSaveLoadRoundTrip() {
    Player player;
    WorldState world;
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    SaveManager saveManager;

    world.setStage(5);
    world.setTurnCount(12);
    world.setResource(ResourceType::Food, 4);
    world.setResource(ResourceType::Water, 7);
    world.setResource(ResourceType::Morale, 9);
    world.setResource(ResourceType::MigrationSupply, 6);
    world.setFlag("flag_base_open");
    world.setFlag("flag_chip_found");
    world.setFlag("flag_pending_event_hidden_orchard");

    player.changeHealth(-30);
    player.changeStamina(-20);
    player.changeStrength(2);
    player.changeWisdom(1);
    player.changeReputation(18);
    player.changeSkillLevel(SkillType::Gather, 1);
    player.changeSkillLevel(SkillType::Combat, 1);
    player.setCurrentRoomId("room_base");
    expect(player.addItem(Item("item_fruit", "果实", false, 2)),
           "fruit setup failed");
    expect(player.addItem(Item("item_rope", "绳索", true, 1)),
           "rope setup failed");

    const auto path = tempSavePath();
    expect(saveManager.saveGame(path.string(), ctx), "save should succeed");

    world.setStage(1);
    world.resetTurnCount();
    world.clearFlags();
    world.setResource(ResourceType::Food, 0);
    world.setResource(ResourceType::Water, 0);
    world.setResource(ResourceType::Morale, 0);
    world.setResource(ResourceType::MigrationSupply, 0);
    player.changeHealth(40);
    player.changeStamina(40);
    player.changeStrength(-2);
    player.changeWisdom(-1);
    player.changeReputation(-18);
    player.setCurrentRoomId("room_tree");

    expect(saveManager.loadGame(path.string(), ctx), "load should succeed");

    expect(world.getStage() == 5, "stage should be restored");
    expect(world.getTurnCount() == 12, "turn count should be restored");
    expect(world.getResource(ResourceType::Food) == 4, "food should restore");
    expect(world.getResource(ResourceType::Water) == 7,
           "water should restore");
    expect(world.getResource(ResourceType::Morale) == 9,
           "morale should restore");
    expect(world.getResource(ResourceType::MigrationSupply) == 6,
           "migration supply should restore");
    expect(world.hasFlag("flag_base_open"), "base flag should restore");
    expect(world.hasFlag("flag_chip_found"), "chip flag should restore");
    expect(world.hasFlag("flag_pending_event_hidden_orchard"),
           "pending event flag should restore");

    expect(player.getHealth() == 70, "health should restore");
    expect(player.getStamina() == 40, "stamina should restore");
    expect(player.getStrength() == 3, "strength should restore");
    expect(player.getWisdom() == 2, "wisdom should restore");
    expect(player.getReputation() == 18, "reputation should restore");
    expect(player.getCurrentRoomId() == "room_base", "room should restore");
    expect(player.getSkillLevel(SkillType::Gather) == 2,
           "gather skill should restore");
    expect(player.getSkillLevel(SkillType::Combat) == 2,
           "combat skill should restore");
    expect(player.hasItem("item_fruit"), "fruit should restore");
    expect(player.hasItem("item_rope"), "rope should restore");
    expect(player.getInventory().getItems().size() == 2,
           "inventory size should restore");

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

void testWorldStateProgression() {
    WorldState world;
    expect(world.getStage() == 1, "initial stage mismatch");
    expect(world.getTurnCount() == 0, "initial turn mismatch");

    world.advanceStage();
    world.consumeTurn();
    expect(world.getStage() == 2, "advanceStage should move to next stage");
    expect(world.getTurnCount() == 1, "consumeTurn should add one turn");

    world.setTurnCount(-5);
    world.setResource(ResourceType::Food, -3);
    world.setFlag("flag_demo");
    world.clearFlags();
    expect(world.getTurnCount() == 0, "turn count should clamp to zero");
    expect(world.getResource(ResourceType::Food) == 0,
           "resource should clamp to zero");
    expect(!world.hasFlag("flag_demo"), "clearFlags should remove flags");
}

void testProgressSystemAppliesSuccessfulResultsOnly() {
    WorldState world;
    ProgressSystem progress;

    ActionResult failed{false, "failed", true, true};
    progress.applyActionResult(failed, world);
    expect(world.getTurnCount() == 0,
           "failed action should not consume turn");
    expect(world.getStage() == 1,
           "failed action should not advance stage");

    ActionResult consumed{true, "moved", true, false};
    progress.applyActionResult(consumed, world);
    expect(world.getTurnCount() == 1,
           "successful consumed action should add turn");
    expect(world.getStage() == 1,
           "non-stage action should not advance stage");

    ActionResult completed{true, "stage done", true, true};
    progress.applyActionResult(completed, world);
    expect(world.getTurnCount() == 2,
           "completed action should still consume turn");
    expect(world.getStage() == 2,
           "completed action should advance one stage");

    world.setStage(6);
    progress.applyActionResult(completed, world);
    expect(world.getStage() == 6,
           "stage should remain capped at final stage");
}

void testStatusViewContainsCoreState() {
    Player player;
    WorldState world;
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};

    world.setStage(3);
    world.setTurnCount(7);
    world.setResource(ResourceType::Food, 2);
    world.setResource(ResourceType::Water, 4);
    world.setFlag("flag_chip_found");
    player.changeHealth(-10);
    player.changeStamina(-5);
    player.changeReputation(12);
    player.changeSkillLevel(SkillType::Leadership, 1);
    player.setCurrentRoomId("room_river");

    const std::string status = buildStatusText(ctx);
    expect(status.find("阶段：3") != std::string::npos,
           "status should show stage");
    expect(status.find("回合：7") != std::string::npos,
           "status should show turns");
    expect(status.find("room_river") != std::string::npos,
           "status should show current room id");
    expect(status.find("生命：90") != std::string::npos,
           "status should show health");
    expect(status.find("体力：55") != std::string::npos,
           "status should show stamina");
    expect(status.find("声望：12") != std::string::npos,
           "status should show reputation");
    expect(status.find("食物2") != std::string::npos,
           "status should show food");
    expect(status.find("水源4") != std::string::npos,
           "status should show water");
    expect(status.find("领导2") != std::string::npos,
           "status should show leadership skill");
    expect(status.find("flag_chip_found") != std::string::npos,
           "status should show key flags");
}

void testEndingJudgement() {
    Player player;
    WorldState world;
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    EndingSystem endings;
    EventSystem story;
    story.initializeEvents();

    world.setFlag("flag_choice_resist");
    expect(endings.determineEndingId(ctx) == "ending_resist",
           "resist ending id mismatch");
    expect(story.getEndingText("ending_resist").find("青木英雄") !=
               std::string::npos,
           "resist ending text missing");

    world = WorldState{};
    world.setFlag("flag_choice_hack");
    expect(endings.determineEndingId(ctx) == "ending_hack",
           "hack ending id mismatch");
    expect(story.getEndingText("ending_hack").find("无声胜利") !=
               std::string::npos,
           "hack ending text missing");

    world = WorldState{};
    world.setFlag("flag_choice_migrate");
    expect(endings.determineEndingId(ctx) == "ending_migrate",
           "migrate ending id mismatch");
    expect(story.getEndingText("ending_migrate").find("向南的新生") !=
               std::string::npos,
           "migrate ending text missing");

    world = WorldState{};
    expect(endings.determineEndingId(ctx) == "ending_fail",
           "failure ending id mismatch");
    expect(story.getEndingText("ending_fail").find("失落之谷") !=
               std::string::npos,
           "failure ending text missing");
}

}  // namespace

int main() {
    try {
        testWorldStateProgression();
        testProgressSystemAppliesSuccessfulResultsOnly();
        testStatusViewContainsCoreState();
        testSaveLoadRoundTrip();
        testEndingJudgement();
    } catch (const std::exception& error) {
        std::cerr << "member5_state_save_test failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "member5_state_save_test passed\n";
    return 0;
}
