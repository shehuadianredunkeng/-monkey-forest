#include "ProgressSystem.h"

#include "WorldState.h"

ActionResult ProgressSystem::applyActionResult(const ActionResult& result,
                                               WorldState& world) const {
    if (!result.success) {
        return result;
    }

    if (result.turnConsumed) {
        world.consumeTurn();
    }
    if (result.stageCompleted) {
        world.advanceStage();
    }
    return result;
}
