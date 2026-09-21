#ifndef MEMBER4_MAIN_SOURCE
#define MEMBER4_MAIN_SOURCE "../src/main.cpp"
#endif
#define main member4_embedded_game_main
#include MEMBER4_MAIN_SOURCE
#undef main

#include "TestFramework.h"

#include <stdexcept>

using namespace std;

namespace {
struct MainFixture {
    Player player;
    WorldState world;
    map<string, Room> rooms;
    GameContext ctx{player, world, rooms};

    explicit MainFixture(vector<string> ids) {
        rooms.emplace("room_tree", Room("room_tree", "树屋", "", {}, {},
                                         move(ids), ""));
    }
};

void testMainMarksEachPickupAndConsumesOneTurn() {
    MainFixture f({"item_fruit", "item_herb", "item_rope"});
    const auto result = takeItemOnce("", f.ctx);
    expect(result.success && result.turnConsumed && !result.stageCompleted,
                "main batch adapter must succeed");
    expect(f.world.getTurnCount() == 0, "pickup itself must not consume turns");
    for (const auto* id : {"item_fruit", "item_herb", "item_rope"}) {
        expect(f.player.hasItem(id), "batch must add every item");
        expect(f.world.hasFlag(string("flag_taken_room_tree_") + id),
                    "main must mark each obtained canonical ID");
    }
    expect(!f.world.hasFlag("flag_taken_room_tree_"), "never mark an empty target");
    ProgressSystem{}.applyActionResult(result, f.world);
    expect(f.world.getTurnCount() == 1 && f.world.getStage() == 1,
                "entire batch must count as one turn without advancing story");
    const auto again = takeItemOnce("", f.ctx);
    ProgressSystem{}.applyActionResult(again, f.world);
    expect(!again.success && !again.turnConsumed && f.world.getTurnCount() == 1 &&
                    f.player.getInventory().getItems()[0].getCount() == 1,
                "repeated batch must not duplicate items or turns");
}

void testMainAliasesShareCanonicalFlag() {
    MainFixture f({"item_fruit", "item_herb"});
    expect(takeItemOnce("herb", f.ctx).success, "short name must work");
    expect(f.world.hasFlag("flag_taken_room_tree_item_herb") &&
                    !f.world.hasFlag("flag_taken_room_tree_herb"), "flags must use canonical ID");
    expect(!takeItemOnce("草药", f.ctx).success, "Chinese alias must not bypass pickup flag");
    expect(!takeItemOnce("item_herb", f.ctx).success, "formal ID must not bypass pickup flag");
    expect(takeItemOnce("", f.ctx).success && f.player.hasItem("item_fruit"),
                "batch must still pick other untaken items");
    expect(f.player.getInventory().getItems()[0].getCount() == 1,
                "already-taken herb must not stack again");
}

void testMainPartialPickupCanBeRetried() {
    MainFixture f({"item_fruit", "item_herb", "item_rope"});
    for (int i = 0; i < 7; ++i) {
        expect(f.player.addItem(Item("test_" + to_string(i), "测试物品")), "fill slots");
    }
    const auto first = takeItemOnce("", f.ctx);
    expect(first.success && f.world.hasFlag("flag_taken_room_tree_item_fruit") &&
                    !f.world.hasFlag("flag_taken_room_tree_item_herb") &&
                    !f.world.hasFlag("flag_taken_room_tree_item_rope"),
                "partial pickup must mark only obtained items");
    expect(f.player.removeItem("test_0") && f.player.removeItem("test_1"), "free two slots");
    const auto second = takeItemOnce("", f.ctx);
    expect(second.success && f.player.hasItem("item_herb") && f.player.hasItem("item_rope"),
                "blocked items must remain available for retry");
    for (const auto& item : f.player.getInventory().getItems()) {
        if (item.getId() == "item_fruit") expect(item.getCount() == 1, "do not repeat fruit");
    }
    ProgressSystem progress;
    progress.applyActionResult(first, f.world);
    progress.applyActionResult(second, f.world);
    expect(f.world.getTurnCount() == 2, "two successful commands must consume two turns");
}

void testMainFullBagAndEmptyRoomDoNotMarkItems() {
    MainFixture full({"item_fruit"});
    for (int i = 0; i < 8; ++i) full.player.addItem(Item("test_" + to_string(i), "测试"));
    const auto result = takeItemOnce("", full.ctx);
    expect(!result.success && !result.turnConsumed && full.world.getFlags().empty(),
                "failed pickup must not mark flags");
    MainFixture empty({});
    expect(!takeItemOnce("", empty.ctx).success && empty.world.getFlags().empty(),
                "empty room must not create a pickup flag");
}

string playScript(const string& script) {
    istringstream input(script);
    ostringstream output;
    auto* oldInput = cin.rdbuf(input.rdbuf());
    auto* oldOutput = cout.rdbuf(output.rdbuf());
    cin.clear();
    try {
        const int result = member4_embedded_game_main();
        expect(result == 0, "game must exit normally");
    } catch (...) {
        cin.rdbuf(oldInput);
        cout.rdbuf(oldOutput);
        cin.clear();
        throw;
    }
    cin.rdbuf(oldInput);
    cout.rdbuf(oldOutput);
    cin.clear();
    return output.str();
}

void testRealMainAcceptsBareTake() {
    const auto text = playScript("go east\ntake\nbag\nstatus\ntake\nstatus\nquit\n");
    expect(text.find("背包 2/8") != string::npos &&
                    text.find("果实 x1") != string::npos && text.find("藤索 x1") != string::npos,
                "bare take must reach batch action through the real input loop");
    const auto first = text.find("回合：2");
    expect(first != string::npos && text.find("回合：2", first + 1) != string::npos,
                "move plus batch is two turns; repeated pickup is free");
}

void testRealMainSupportsItemAliases() {
    const auto text = playScript("go east\ntake fruit\ntake 果实\nuse 果实\nbag\nstatus\nquit\n");
    expect(text.find("背包 0/8") != string::npos && text.find("回合：3") != string::npos,
                "move + specified pickup + use are three turns without alias duplication");
}

void testTakeDoesNotAcceptCommandChains() {
    const auto text = playScript("go east\ntake fruit && use fruit\ntake fruit; use fruit\n"
                                 "take fruit, use fruit\nbag\nstatus\nquit\n");
    expect(text.find("背包 0/8") != string::npos && text.find("回合：1") != string::npos,
                "malformed chained take must not execute pickup or use");
}
} // namespace

int main() {
    const pair<const char*, void (*)()> tests[] = {
        {"main batch flags/turn", testMainMarksEachPickupAndConsumesOneTurn},
        {"main alias flags", testMainAliasesShareCanonicalFlag},
        {"main partial retry", testMainPartialPickupCanBeRetried},
        {"main full/empty", testMainFullBagAndEmptyRoomDoNotMarkItems},
        {"real main bare take", testRealMainAcceptsBareTake},
        {"real main aliases", testRealMainSupportsItemAliases},
        {"real main no chains", testTakeDoesNotAcceptCommandChains},
    };
    int failed = 0;
    for (const auto& test : tests) {
        try { test.second(); }
        catch (const exception& error) {
            ++failed;
            cerr << test.first << ": " << error.what() << '\n';
        }
    }
    cout << "member4 main: " << (sizeof(tests) / sizeof(tests[0]) - failed)
              << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
