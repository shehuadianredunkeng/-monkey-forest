#pragma once

#include "CommonTypes.h"

class WorldState;

class ProgressSystem {
public:
    ActionResult applyActionResult(const ActionResult& result,
                                   WorldState& world) const;
};
