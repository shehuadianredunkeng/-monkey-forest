#pragma once

#include <map>
#include <string>

using namespace std;

class Player;
class Room;
class WorldState;

enum class SkillType {
    Climb,
    Combat,
    Leadership
};

enum class ResourceType {
    Food,
    Water,
    Morale,
    MigrationSupply
};

struct ActionResult {
    bool success = false;
    string message;
    bool turnConsumed = false;
    bool stageCompleted = false;
};

struct GameContext {
    Player& player;
    WorldState& world;
    map<string, Room>& rooms;
};
