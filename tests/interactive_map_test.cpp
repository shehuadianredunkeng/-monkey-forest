#include "InteractiveMap.h"

#include "Player.h"
#include "Room.h"
#include "WorldState.h"

#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

namespace {
void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::string terrainSignature(const InteractiveMap& map) {
    std::string signature;
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

        const std::string roomIds[] = {
            "room_tree", "room_forest", "room_river", "room_cave", "room_base"};
        std::set<std::string> terrainLayouts;
        for (const std::string& roomId : roomIds) {
            player.setCurrentRoomId(roomId);
            map.resetForRoom(ctx);
            const std::string signature = terrainSignature(map);
            expect(signature.find('#') != std::string::npos,
                   "every room should contain solid terrain");
            for (int y = 0; y < map.height(); ++y)
                for (int x = 0; x < map.width(); ++x) {
                    const std::wstring glyph = map.visualAt(x, y, ctx).glyph;
                    const bool interactive =
                        glyph != L"##" && glyph != L"~~" && glyph != L"··";
                    if (interactive) {
                        const char floor = map.terrainAt(x, y);
                        if (floor == '#' || floor == '~')
                            throw std::runtime_error(
                                "interactive marker hidden in terrain: " + roomId +
                                " (" + std::to_string(x) + "," +
                                std::to_string(y) + ")");
                    }
                }
            terrainLayouts.insert(signature);
        }
        expect(terrainLayouts.size() == 5,
               "all five rooms should have distinct terrain layouts");
        player.setCurrentRoomId("room_river");
        map.resetForRoom(ctx);
        expect(terrainSignature(map).find('~') != std::string::npos,
               "river room should contain a visible river");

        player.setCurrentRoomId("room_tree");
        map.resetForRoom(ctx);
        const std::string terrainBeforeWalking = terrainSignature(map);
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
                const std::wstring glyph = map.visualAt(x, y, ctx).glyph;
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
                   std::string::npos,
               "failed transition must explain the stamina requirement");

        std::cout << "interactive_map_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "interactive_map_test failed: " << error.what() << '\n';
        return 1;
    }
}
