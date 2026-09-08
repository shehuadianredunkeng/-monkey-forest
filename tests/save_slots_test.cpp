#include "Player.h"
#include "Room.h"
#include "SaveManager.h"
#include "SaveSlots.h"
#include "WorldState.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    const std::filesystem::path folder = "save_slots_test_data";
    try {
        std::filesystem::remove_all(folder);
        Player player;
        player.setCurrentRoomId("room_river");
        WorldState world;
        world.setStage(3);
        world.setTurnCount(12);
        auto rooms = createAllRooms();
        GameContext ctx{player, world, rooms};
        SaveManager manager;
        SaveSlots slots(folder.string());

        expect(!slots.any(), "fresh slots should be empty");
        expect(slots.save(2, ctx, manager), "slot 2 save failed");
        expect(slots.exists(2) && slots.any(), "saved slot not detected");
        expect(slots.descriptions().at(1).find("清泉河谷") != std::string::npos,
               "slot description lacks room name");

        Player loadedPlayer;
        WorldState loadedWorld;
        auto loadedRooms = createAllRooms();
        GameContext loaded{loadedPlayer, loadedWorld, loadedRooms};
        expect(slots.load(2, loaded, manager), "slot 2 load failed");
        expect(loadedWorld.getStage() == 3 &&
               loadedPlayer.getCurrentRoomId() == "room_river",
               "slot data changed during load");

        std::filesystem::remove_all(folder);
        std::cout << "save_slots_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::filesystem::remove_all(folder);
        std::cerr << "save_slots_test failed: " << error.what() << '\n';
        return 1;
    }
}
