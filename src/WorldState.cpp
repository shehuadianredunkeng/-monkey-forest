#include "WorldState.h"

#include <algorithm>

namespace {

constexpr int kFirstStage = 1;
constexpr int kFinalStage = 6;

int clampToNonNegative(int value) {
    return std::max(value, 0);
}

int resourceValue(const std::map<ResourceType, int>& resources,
                  ResourceType type) {
    const auto it = resources.find(type);
    return it == resources.end() ? 0 : it->second;
}

}  // namespace

int WorldState::getStage() const {
    return stage_;
}

void WorldState::setStage(int stage) {
    stage_ = std::clamp(stage, kFirstStage, kFinalStage);
}

void WorldState::advanceStage() {
    setStage(stage_ + 1);
}

int WorldState::getTurnCount() const {
    return turnCount_;
}

void WorldState::setTurnCount(int turnCount) {
    turnCount_ = clampToNonNegative(turnCount);
}

void WorldState::consumeTurn() {
    ++turnCount_;
}

void WorldState::resetTurnCount() {
    turnCount_ = 0;
}

int WorldState::getResource(ResourceType type) const {
    return resourceValue(resources_, type);
}

void WorldState::setResource(ResourceType type, int value) {
    resources_[type] = clampToNonNegative(value);
}

void WorldState::changeResource(ResourceType type, int delta) {
    setResource(type, getResource(type) + delta);
}

bool WorldState::hasFlag(const std::string& flag) const {
    return flags_.count(flag) != 0;
}

void WorldState::setFlag(const std::string& flag) {
    flags_.insert(flag);
}

void WorldState::removeFlag(const std::string& flag) {
    flags_.erase(flag);
}

void WorldState::clearFlags() {
    flags_.clear();
}

std::vector<std::string> WorldState::getFlags() const {
    return {flags_.begin(), flags_.end()};
}
