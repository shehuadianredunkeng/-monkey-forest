#include "TestSupport.h"

#include <map>

namespace {
struct TestPlayerState {
    int stamina = 0;
    std::map<SkillType, int> skills;
    std::string roomId;
};

std::map<const Player*, TestPlayerState> playerStates;
std::map<const WorldState*, std::map<std::string, bool>> worldFlags;
std::map<const WorldState*, int> worldStages;
}

int Player::getStamina() const {
    return playerStates[this].stamina;
}

void Player::changeStamina(int delta) {
    playerStates[this].stamina += delta;
}

int Player::getSkillLevel(SkillType skill) const {
    return playerStates[this].skills[skill];
}

const std::string& Player::getCurrentRoomId() const {
    return playerStates[this].roomId;
}

void Player::setCurrentRoomId(const std::string& roomId) {
    playerStates[this].roomId = roomId;
}

void setTestPlayerStamina(Player& player, int stamina) {
    playerStates[&player].stamina = stamina;
}

void setTestPlayerSkill(Player& player, SkillType skill, int level) {
    playerStates[&player].skills[skill] = level;
}

bool WorldState::hasFlag(const std::string& flag) const {
    return worldFlags[this][flag];
}

int WorldState::getStage() const {
    return worldStages[this] == 0 ? 1 : worldStages[this];
}

void setTestWorldFlag(WorldState& world, const std::string& flag, bool enabled) {
    worldFlags[&world][flag] = enabled;
}
