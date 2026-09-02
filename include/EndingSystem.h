#pragma once

#include "CommonTypes.h"

#include <string>

class EndingSystem {
public:
    std::string determineEndingId(const GameContext& ctx) const;
};
