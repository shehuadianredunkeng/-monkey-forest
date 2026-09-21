#include "ProgressSystem.h"

#include "WorldState.h"

using namespace std;

ActionResult ProgressSystem::applyActionResult(const ActionResult& result,
                                               WorldState& world) const {
    // 地图和事件只返回动作结果，回合结算集中在这里，避免重复扣回合。
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
