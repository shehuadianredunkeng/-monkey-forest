#include "Player.h"
#include "Room.h"
#include "SaveManager.h"
#include "SaveSlots.h"
#include "TestFramework.h"
#include "WorldState.h"

#include <filesystem>
#include <chrono>
#include <iostream>
#include <stdexcept>

using namespace std;

int main() {
    const filesystem::path folder = "save_slots_test_data";
    try {
        filesystem::remove_all(folder);
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
        filesystem::last_write_time(
            folder / "slot2.txt",
            filesystem::file_time_type::clock::now() -
                chrono::seconds(10));
        expect(slots.save(1, ctx, manager), "newest slot setup failed");
        expect(slots.mostRecentSlot() == 1,
               "most recent save slot should be preferred after an ending");
        expect(slots.descriptions(ctx.rooms).at(1).find("清泉河谷") != string::npos,
               "slot description lacks room name");

        Player loadedPlayer;
        WorldState loadedWorld;
        auto loadedRooms = createAllRooms();
        GameContext loaded{loadedPlayer, loadedWorld, loadedRooms};
        expect(slots.load(2, loaded, manager), "slot 2 load failed");
        expect(loadedWorld.getStage() == 3 &&
               loadedPlayer.getCurrentRoomId() == "room_river",
               "slot data changed during load");

        filesystem::remove_all(folder);
        cout << "save_slots_test passed\n";
        return 0;
    } catch (const exception& error) {
        filesystem::remove_all(folder);
        cerr << "save_slots_test failed: " << error.what() << '\n';
        return 1;
    }
}
