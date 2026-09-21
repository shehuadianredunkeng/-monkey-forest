#pragma once

#include "CommonTypes.h"
#include "Room.h"

#include <map>
#include <string>
#include <vector>

using namespace std;

class SaveManager;
class WorldState;

class SaveSlots {
public:
    explicit SaveSlots(string folder = "saves");

    bool any() const;
    bool exists(int slot) const;
    int mostRecentSlot() const;
    bool save(int slot, const GameContext& ctx, const SaveManager& manager) const;
    bool load(int slot, GameContext& ctx, const SaveManager& manager) const;
    vector<string> descriptions(
        const map<string, Room>& rooms) const;

private:
    string folder_;
    string path(int slot) const;
};

void mergeCollectionFlags(const WorldState& from, WorldState& into);
bool loadCollectionProfile(const string& path, WorldState& profile);
bool saveCollectionProfile(const string& path, const WorldState& profile);
