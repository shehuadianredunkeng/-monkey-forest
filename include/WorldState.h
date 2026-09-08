#pragma once

#include "CommonTypes.h"

#include <map>
#include <set>
#include <string>
#include <vector>

class WorldState {
public:
    int getStage() const;
    void setStage(int stage);
    void advanceStage();

    int getTurnCount() const;
    void setTurnCount(int turnCount);
    void consumeTurn();
    void resetTurnCount();

    int getResource(ResourceType type) const;
    void setResource(ResourceType type, int value);
    void changeResource(ResourceType type, int delta);

    bool hasFlag(const std::string& flag) const;
    void setFlag(const std::string& flag);
    void removeFlag(const std::string& flag);
    void clearFlags();
    std::vector<std::string> getFlags() const;

private:
    int stage_ = 1;
    int turnCount_ = 0;
    std::map<ResourceType, int> resources_;
    std::set<std::string> flags_;
};
