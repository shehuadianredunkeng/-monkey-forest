#pragma once

#include "CommonTypes.h"

#include <string>

class WorldState {
public:
    int getStage() const;
    void setStage(int stage);

    int getTurnCount() const;
    void consumeTurn();
    void resetTurnCount();

    int getResource(ResourceType type) const;
    void changeResource(ResourceType type, int delta);

    bool hasFlag(const std::string& flag) const;
    void setFlag(const std::string& flag);
    void removeFlag(const std::string& flag);
};
