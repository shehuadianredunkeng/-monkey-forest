#pragma once

#include "CommonTypes.h"

#include <string>

using namespace std;

class EndingSystem {
public:
    string determineEndingId(const GameContext& ctx) const;
};
