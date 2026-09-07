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

        for (int i = 0; i < 4; ++i) expect(map.move(0, -1, ctx).moved,
                                           "cannot reach tree corridor");
        MapMoveResult transition;
        for (int i = 0; i < 18; ++i) transition = map.move(1, 0, ctx);
        expect(transition.roomChanged, "walking into door did not switch room");
        expect(player.getCurrentRoomId() == "room_forest",
               "door did not use room connection interface");
        expect(map.visualAt(18, 5, ctx).color == UI::Color::Quest,
               "main quest marker missing");

        for (int i = 0; i < 5; ++i) map.move(1, 0, ctx);
        map.move(0, -1, ctx);
        const MapInteraction scout = map.interact(ctx);
        expect(scout.kind == InteractionKind::Npc && scout.id == "npc_scout",
               "proximity interaction did not find NPC");

        std::cout << "interactive_map_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "interactive_map_test failed: " << error.what() << '\n';
        return 1;
    }
}
