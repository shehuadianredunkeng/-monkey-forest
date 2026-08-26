#pragma once

#include <map>
#include <string>

class Player;
class Room;
class WorldState;

enum class SkillType {
    Gather,
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
    std::string message;
    bool turnConsumed = false;
    bool stageCompleted = false;
};

struct GameContext {
    Player& player;
    WorldState& world;
    std::map<std::string, Room>& rooms;
};
