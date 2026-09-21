#pragma once

#include "CommonTypes.h"

#include <string>

using namespace std;

class SaveManager {
public:
    bool saveGame(const string& path, const GameContext& ctx) const;
    bool loadGame(const string& path, GameContext& ctx) const;
};
