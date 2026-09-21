#pragma once

#include "CommonTypes.h"

using namespace std;

class WorldState;

class ProgressSystem {
public:
    ActionResult applyActionResult(const ActionResult& result,
                                   WorldState& world) const;
};
