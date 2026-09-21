#pragma once

#include "CommonTypes.h"

#include <map>
#include <set>
#include <string>
#include <vector>

using namespace std;

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

    bool hasFlag(const string& flag) const;
    void setFlag(const string& flag);
    void removeFlag(const string& flag);
    void clearFlags();
    vector<string> getFlags() const;

private:
    int stage_ = 1;
    int turnCount_ = 0;
    map<ResourceType, int> resources_;
    set<string> flags_;
};
