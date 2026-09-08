#include "InteractiveMap.h"

#include "Player.h"
#include "Room.h"
#include "WorldState.h"

#include <iostream>
#include <stdexcept>

namespace {
void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
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
