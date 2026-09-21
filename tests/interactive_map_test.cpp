#include "InteractiveMap.h"
#include "TestFramework.h"

#include "Player.h"
#include "Room.h"
#include "WorldState.h"

#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

using namespace std;

namespace {
string terrainSignature(const InteractiveMap& map) {
    string signature;
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x)
            signature.push_back(map.terrainAt(x, y));
    return signature;
}
}

int main() {
    try {
        Player player;
        WorldState world;
        world.setStage(1);
        auto rooms = createAllRooms();
        GameContext ctx{player, world, rooms};
        InteractiveMap map;
        map.resetForRoom(ctx);

        const string roomIds[] = {
            "room_tree", "room_forest", "room_river", "room_cave", "room_base"};
        set<string> terrainLayouts;
        for (const string& roomId : roomIds) {
            player.setCurrentRoomId(roomId);
            map.resetForRoom(ctx);
            const string signature = terrainSignature(map);
            expect(signature.find('#') != string::npos,
                   "every room should contain solid terrain");
            for (int y = 0; y < map.height(); ++y)
                for (int x = 0; x < map.width(); ++x) {
                    const wstring glyph = map.visualAt(x, y, ctx).glyph;
                    const bool interactive =
                        glyph != L"##" && glyph != L"~~" && glyph != L"··";
                    if (interactive) {
                        const char floor = map.terrainAt(x, y);
                        if (floor == '#' || floor == '~')
                            throw runtime_error(
                                "interactive marker hidden in terrain: " + roomId +
                                " (" + to_string(x) + "," +
                                to_string(y) + ")");
                    }
                }
            terrainLayouts.insert(signature);
        }
        expect(terrainLayouts.size() == 5,
               "all five rooms should have distinct terrain layouts");
        struct NpcMarker { const char* room; int x; int y; const wchar_t* glyph; };
        for (const auto& marker : {
                 NpcMarker{"room_tree", 7, 5, L"岩"},
                 NpcMarker{"room_tree", 11, 10, L"叶"},
                 NpcMarker{"room_forest", 6, 5, L"闪"},
                 NpcMarker{"room_river", 12, 10, L"豆"},
                 NpcMarker{"room_base", 27, 7, L"赫"}}) {
            player.setCurrentRoomId(marker.room);
            map.resetForRoom(ctx);
            expect(map.visualAt(marker.x, marker.y, ctx).glyph == marker.glyph,
                   "NPC marker should show the first character of its name");
        }
        player.setCurrentRoomId("room_river");
        map.resetForRoom(ctx);
        expect(terrainSignature(map).find('~') != string::npos,
               "river room should contain a visible river");

        world.setFlag("flag_child_rescued");
        player.setCurrentRoomId("room_river");
        map.resetForRoom(ctx);
        int riverFriendsAfterRescue = 0;
        for (int y = 0; y < map.height(); ++y)
            for (int x = 0; x < map.width(); ++x)
                if (map.visualAt(x, y, ctx).glyph == L"豆") ++riverFriendsAfterRescue;
        expect(riverFriendsAfterRescue == 0,
               "rescued child should no longer be visible in river map");

        player.setCurrentRoomId("room_tree");
        map.resetForRoom(ctx);
        int treeFriendsAfterRescue = 0;
        for (int y = 0; y < map.height(); ++y)
            for (int x = 0; x < map.width(); ++x)
                if (set<wstring>{L"岩", L"叶", L"豆"}.count(map.visualAt(x, y, ctx).glyph)) ++treeFriendsAfterRescue;
        expect(map.visualAt(24, 10, ctx).glyph == L"豆",
               "rescued child should retain its name marker");
        expect(treeFriendsAfterRescue >= 3,
               "rescued child should appear beside king and healer in tree map");
        world.removeFlag("flag_child_rescued");

        player.setCurrentRoomId("room_tree");
        map.resetForRoom(ctx);
        const string terrainBeforeWalking = terrainSignature(map);
        expect(map.visualAt(1, 1, ctx).glyph == L"··",
               "floor tile should occupy the complete two-column map cell");
        map.move(0, -1, ctx);
        map.move(0, 1, ctx);
        map.move(-1, 0, ctx);
        map.move(1, 0, ctx);
        expect(terrainSignature(map) == terrainBeforeWalking,
               "walking must never erase walls, water, or floor terrain");

        expect(map.width() == 36 && map.height() == 15,
               "interactive room dimensions changed");
        expect(map.visualAt(map.playerX(), map.playerY(), ctx).glyph == L"猴",
               "player marker missing");
        int roamingEnemies = 0;
        int locationEvents = 0;
        for (int y = 0; y < map.height(); ++y)
            for (int x = 0; x < map.width(); ++x) {
                const wstring glyph = map.visualAt(x, y, ctx).glyph;
                if (glyph == L"敌") ++roamingEnemies;
                if (glyph == L"奇") ++locationEvents;
            }
        expect(roamingEnemies >= 2, "each turn should populate multiple enemies");
        expect(locationEvents == 1, "each room should expose one turn event");

        const int staminaBeforeWalking = player.getStamina();
        for (int i = 0; i < 4; ++i) expect(map.move(0, -1, ctx).moved,
                                           "cannot reach tree corridor");
        expect(player.getStamina() == staminaBeforeWalking,
               "walking inside a room must not consume stamina");
        MapMoveResult transition;
        for (int i = 0; i < 18; ++i) transition = map.move(1, 0, ctx);
        expect(transition.roomChanged, "walking into door did not switch room");
        expect(player.getStamina() == staminaBeforeWalking - 3,
               "successful room transition must consume exactly three stamina");
        expect(player.getCurrentRoomId() == "room_forest",
               "door did not use room connection interface");
        expect(map.visualAt(18, 5, ctx).color == UI::Color::Quest,
               "main quest marker missing");

        for (int i = 0; i < 5; ++i) map.move(1, 0, ctx);
        map.move(0, -1, ctx);
        const MapInteraction scout = map.interact(ctx);
        expect(scout.kind == InteractionKind::Npc && scout.id == "npc_scout",
               "proximity interaction did not find NPC");
        const int savedX = map.playerX();
        const int savedY = map.playerY();
        map.storePosition(ctx);
        InteractiveMap restored;
        restored.resetForRoom(ctx);
        expect(restored.playerX() == savedX && restored.playerY() == savedY,
               "saved tile position was not restored");

        Player lockedPlayer;
        WorldState lockedWorld;
        lockedPlayer.setCurrentRoomId("room_river");
        GameContext lockedCtx{lockedPlayer, lockedWorld, rooms};
        InteractiveMap lockedMap;
        lockedMap.resetForRoom(lockedCtx);
        const MapTileVisual lockedDoor = lockedMap.visualAt(35, 7, lockedCtx);
        expect(lockedDoor.glyph == L"锁" && lockedDoor.color == UI::Color::Error,
               "locked base door must be visibly red");

        Player cavePlayer;
        WorldState caveWorld;
        cavePlayer.setCurrentRoomId("room_cave");
        GameContext caveCtx{cavePlayer, caveWorld, rooms};
        InteractiveMap caveMap;
        caveMap.resetForRoom(caveCtx);
        expect(caveMap.visualAt(35, 7, caveCtx).glyph != L"门",
               "dead-end cave door should not be drawn");

        Player tiredPlayer;
        tiredPlayer.changeStamina(-58);
        WorldState tiredWorld;
        tiredWorld.setStage(1);
        GameContext tiredCtx{tiredPlayer, tiredWorld, rooms};
        InteractiveMap tiredMap;
        tiredMap.resetForRoom(tiredCtx);
        for (int i = 0; i < 4; ++i) tiredMap.move(0, -1, tiredCtx);
        MapMoveResult blockedTransition;
        for (int i = 0; i < 18; ++i)
            blockedTransition = tiredMap.move(1, 0, tiredCtx);
        expect(!blockedTransition.roomChanged,
               "room transition must fail when stamina is below three");
        expect(tiredPlayer.getCurrentRoomId() == "room_tree",
               "failed transition must keep the current room");
        expect(tiredPlayer.getStamina() == 2,
               "failed transition must not consume stamina");
        expect(blockedTransition.action.message.find("需要 3 点体力") !=
                   string::npos,
               "failed transition must explain the stamina requirement");

        cout << "interactive_map_test passed\n";
        return 0;
    } catch (const exception& error) {
        cerr << "interactive_map_test failed: " << error.what() << '\n';
        return 1;
    }
}
