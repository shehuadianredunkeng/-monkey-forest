#include "CombatSystem.h"

#include <algorithm>

#include "Player.h"
#include "WorldState.h"

// 由4号模块提供。战斗中的 use 命令通过此接口复用统一物品逻辑。
ActionResult useItem(const std::string& itemId, GameContext& ctx);

void CombatSystem::initializeEnemies() {
    enemies_.clear();
    enemies_.emplace("enemy_bees", Enemy{"enemy_bees", "野蜂群",
        "守护果树的躁动蜂群。", 18, 6, 0, 5});
    enemies_.emplace("enemy_robot", Enemy{"enemy_robot", "星猿巡逻机",
        "在基地外围巡逻的机械守卫。", 30, 9, 2, 12});
    enemies_.emplace("enemy_hertz", Enemy{"enemy_hertz", "赫兹",
        "穿着能源护甲的星猿工程官。", 42, 11, 3, 20});
}

const Enemy* CombatSystem::currentEnemy() const {
    const auto it = enemies_.find(battleState_.enemyId);
    return it == enemies_.end() ? nullptr : &it->second;
}

ActionResult CombatSystem::startBattle(const std::string& enemyId,
                                       GameContext&) {
    if (enemies_.empty()) initializeEnemies();
    if (battleState_.inBattle)
        return {false, "当前战斗尚未结束。", false, false};
    const auto it = enemies_.find(enemyId);
    if (it == enemies_.end())
        return {false, "找不到这个敌人。", false, false};

    battleState_.inBattle = true;
    battleState_.enemyId = enemyId;
    battleState_.enemyHealth = it->second.getMaxHealth();
    battleState_.playerGuarding = false;
    return {true, "战斗开始：" + it->second.getName() +
                  "，敌方生命 " + std::to_string(battleState_.enemyHealth) + "。",
            false, false};
}

ActionResult CombatSystem::performBattleAction(const std::string& action,
                                               const std::string& target,
                                               GameContext& ctx) {
    if (!battleState_.inBattle)
        return {false, "当前不在战斗中。", false, false};
    const Enemy* enemy = currentEnemy();
    if (!enemy) {
        clearBattle();
        return {false, "战斗数据异常，战斗已结束。", false, false};
    }

    if (action == "attack") {
        const int damage = std::max(1, 3 + ctx.player.getStrength() * 2 +
            ctx.player.getSkillLevel(SkillType::Combat) - enemy->getDefense());
        battleState_.enemyHealth = std::max(0, battleState_.enemyHealth - damage);
        if (battleState_.enemyHealth == 0)
            return finishVictory(ctx, *enemy);
        ActionResult counter = enemyCounterAttack(ctx, false);
        counter.message = "你造成" + std::to_string(damage) + "点伤害。" + counter.message;
        return counter;
    }
    if (action == "guard") {
        battleState_.playerGuarding = true;
        ActionResult counter = enemyCounterAttack(ctx, true);
        counter.message = "你摆出防御姿态。" + counter.message;
        return counter;
    }
    if (action == "use") {
        if (target.empty())
            return {false, "请输入要使用的物品ID。", false, false};
        ActionResult used = useItem(target, ctx);
        if (!used.success) return used;
        ActionResult counter = enemyCounterAttack(ctx, false);
        counter.message = used.message + counter.message;
        return counter;
    }
    if (action == "escape") {
        if (battleState_.enemyId == "enemy_hertz" &&
            !ctx.world.hasFlag("flag_scout_help")) {
            return {false, "赫兹封锁了出口；没有同伴掩护，暂时无法撤退。",
                    false, false};
        }
        clearBattle();
        return {true, "你成功脱离战斗。", true, false};
    }
    return {false, "未知战斗命令，可使用 attack、guard、use 或 escape。",
            false, false};
}

ActionResult CombatSystem::enemyCounterAttack(GameContext& ctx, bool guarded) {
    const Enemy* enemy = currentEnemy();
    if (!enemy) return {false, "敌人数据不存在。", false, false};
    int damage = std::max(1, enemy->getAttack() - ctx.player.getStrength());
    if (guarded) damage = std::max(1, damage / 2);
    ctx.player.changeHealth(-damage);
    battleState_.playerGuarding = false;
    if (ctx.player.getHealth() <= 0) {
        clearBattle();
        return {false, "敌人反击造成" + std::to_string(damage) +
                       "点伤害。你失去了意识。", true, false};
    }
    return {true, "敌人反击造成" + std::to_string(damage) +
                  "点伤害；敌方剩余生命" +
                  std::to_string(battleState_.enemyHealth) + "。", true, false};
}

ActionResult CombatSystem::finishVictory(GameContext& ctx, const Enemy& enemy) {
    ctx.player.changeReputation(enemy.getReputationReward());
    if (enemy.getId() == "enemy_robot") ctx.world.setFlag("flag_robot_defeated");
    if (enemy.getId() == "enemy_hertz") ctx.world.setFlag("flag_hertz_defeated");
    if (enemy.getId() == "enemy_bees") ctx.world.setFlag("flag_bees_defeated");
    const int reward = enemy.getReputationReward();
    const std::string name = enemy.getName();
    clearBattle();
    return {true, "你击败了" + name + "。声望+" + std::to_string(reward) + "。",
            true, false};
}

const BattleState& CombatSystem::getBattleState() const { return battleState_; }
bool CombatSystem::isInBattle() const { return battleState_.inBattle; }

void CombatSystem::clearBattle() { battleState_ = BattleState{}; }
