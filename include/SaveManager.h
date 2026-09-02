#pragma once

#include "CommonTypes.h"

#include <string>

class SaveManager {
public:
    bool saveGame(const std::string& path, const GameContext& ctx) const;
    bool loadGame(const std::string& path, GameContext& ctx) const;
};
