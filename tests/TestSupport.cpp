#include "TestSupport.h"

#include <map>

using namespace std;

namespace {
struct TestPlayerState {
    int stamina = 0;
    int strength = 1;
    map<SkillType, int> skills;
    string roomId;
};

map<const Player*, TestPlayerState> playerStates;
map<const WorldState*, map<string, bool>> worldFlags;
map<const WorldState*, int> worldStages;
}

int Player::getStamina() const {
    return playerStates[this].stamina;
}

void Player::changeStamina(int delta) {
    playerStates[this].stamina += delta;
}

int Player::getStrength() const {
    return playerStates[this].strength;
}

void Player::changeStrength(int delta) {
    playerStates[this].strength += delta;
}

int Player::getSkillLevel(SkillType skill) const {
    return playerStates[this].skills[skill];
}

const string& Player::getCurrentRoomId() const {
    return playerStates[this].roomId;
}

void Player::setCurrentRoomId(const string& roomId) {
    playerStates[this].roomId = roomId;
}

void setTestPlayerStamina(Player& player, int stamina) {
    playerStates[&player].stamina = stamina;
}

void setTestPlayerStrength(Player& player, int strength) {
    playerStates[&player].strength = strength;
}

void setTestPlayerSkill(Player& player, SkillType skill, int level) {
    playerStates[&player].skills[skill] = level;
}

bool WorldState::hasFlag(const string& flag) const {
    return worldFlags[this][flag];
}

int WorldState::getStage() const {
    return worldStages[this] == 0 ? 1 : worldStages[this];
}

void setTestWorldFlag(WorldState& world, const string& flag, bool enabled) {
    worldFlags[&world][flag] = enabled;
}
