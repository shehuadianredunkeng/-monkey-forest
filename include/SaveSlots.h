#pragma once

#include "CommonTypes.h"

#include <string>
#include <vector>

class SaveManager;
class WorldState;

class SaveSlots {
public:
    explicit SaveSlots(std::string folder = "saves");

    bool any() const;
    bool exists(int slot) const;
    bool save(int slot, const GameContext& ctx, const SaveManager& manager) const;
    bool load(int slot, GameContext& ctx, const SaveManager& manager) const;
    std::vector<std::string> descriptions() const;

private:
    std::string folder_;
    std::string path(int slot) const;
};

void mergeCollectionFlags(const WorldState& from, WorldState& into);
bool loadCollectionProfile(const std::string& path, WorldState& profile);
bool saveCollectionProfile(const std::string& path, const WorldState& profile);
