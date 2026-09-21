#define main gameEntryForTest
#include "../src/main.cpp"
#undef main

#include "TestFramework.h"

#include <deque>
#include <iostream>
#include <stdexcept>

using namespace std;

namespace {
struct TestSurface final : UI::ConsoleSurface {
    deque<UI::InputEvent> input;
    wstring output;
    bool closed = false;
    bool prepare() override { return !closed; }
    void clear(UI::Rect) override {}
    void write(UI::SHORT, UI::SHORT, const wstring& text, UI::Color) override {
        output += text;
    }
    void cursor(UI::SHORT, UI::SHORT, bool) override {}
    int measure(const wstring& glyph) override { return UI::portableColumns(glyph); }
    UI::InputEvent readEvent() override {
        if (input.empty()) { closed = true; return {UI::Key::EndOfInput, {}}; }
        auto event = input.front(); input.pop_front(); return event;
    }
    void restore() override {}
};

void roundTrip(const string& enemy, const string& action) {
    Player player; WorldState world; auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    player.changeWisdom(3);
    world.setFlag("flag_complete_log");
    CombatSystem combat; combat.initializeEnemies();
    EventSystem events; events.initializeEvents(); ProgressSystem progress;
    InteractiveMap map; map.resetForRoom(ctx);
    SaveManager manager; SaveSlots slots("roundtrip_saves");
    auto device = make_unique<TestSurface>(); auto* screen = device.get();
    UI::ConsoleRenderer renderer(move(device)); UI::InteractiveGameUI ui(renderer);
    expect(combat.startBattle(enemy, ctx).success, "start failed");
    expect(combat.performBattleAction(action, "", ctx).success, "action failed");
    const auto before = combat.getBattleState();
    const auto hp = player.getHealth(); const auto turns = world.getTurnCount();
    screen->input.push_back({UI::Key::Enter, {}});
    handleBattleCommand("save", ctx, combat, events, progress, ui, map, slots, manager);
    expect(slots.descriptions(ctx.rooms)[0].find("战斗中（敌生命" +
              to_string(before.enemyHealth) + "）") != string::npos,
          "slot should identify battle and saved enemy health");
    expect(world.getTurnCount() == turns && player.getHealth() == hp,
          "saving must not consume a turn or health");

    Player loaded; WorldState loadedWorld; auto loadedRooms = createAllRooms();
    GameContext loadCtx{loaded, loadedWorld, loadedRooms};
    expect(slots.load(1, loadCtx, manager), "file load failed");
    CombatSystem restored; restored.initializeEnemies();
    expect(restored.restoreBattleState(loadCtx), "file battle restore failed");
    const auto after = restored.getBattleState();
    expect(after.enemyHealth == before.enemyHealth && loaded.getHealth() == hp,
          "file roundtrip lost enemy/player health");
    expect(after.enemyId == before.enemyId && after.encounterId == before.encounterId &&
          after.inBattle && after.theftUsed == before.theftUsed &&
          after.consecutiveGuards == before.consecutiveGuards &&
          after.awaitingBananaChoice == before.awaitingBananaChoice,
          "file roundtrip lost battle status");
    combat.performBattleAction("guard", "", ctx);
    restored.performBattleAction("guard", "", loadCtx);
    expect(player.getHealth() == loaded.getHealth() &&
          combat.getBattleState().enemyHealth == restored.getBattleState().enemyHealth,
          "restored next turn differs (armor/turn counter lost)");

    expect(slots.load(1, loadCtx, manager), "second load failed");
    auto startupDevice = make_unique<TestSurface>(); auto* startup = startupDevice.get();
    filesystem::remove("roundtrip_saves/slot1.txt");
    startup->input.push_back({UI::Key::Text, L"P"});
    startup->input.push_back({UI::Key::Enter, {}});
    UI::ConsoleRenderer startupRenderer(move(startupDevice));
    UI::InteractiveGameUI startupUi(startupRenderer);
    WorldState profile; CollectionSystem collections;
    play(loadCtx, startupUi, slots, manager, profile, collections);
    expect(slots.load(1, loadCtx, manager), "startup save failed");
    expect(restored.restoreBattleState(loadCtx) &&
          restored.getBattleState().enemyHealth == before.enemyHealth,
          "play startup reset battle health");
    cout << "PASS disk roundtrip: " << enemy << " / " << action << '\n';
}
}

int main() {
    try {
        roundTrip("enemy_robot", "attack");
        roundTrip("enemy_bees", "guard");
        roundTrip("enemy_hertz", "analyze");
        roundTrip("enemy_drone@room_river@3@3", "steal");
        roundTrip("enemy_season_guardian_spring@room_forest@0", "attack");
    } catch (const exception& e) {
        cerr << e.what() << '\n'; return 1;
    }
}
