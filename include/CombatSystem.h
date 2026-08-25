#pragma once

#include <map>
#include <string>

#include "Enemy.h"
#include "GameContext.h"

struct BattleState {
    bool inBattle = false;
    std::string enemyId;
    int enemyHealth = 0;
    bool playerGuarding = false;
};

class CombatSystem {
public:
    void initializeEnemies();

    ActionResult startBattle(const std::string& enemyId, GameContext& ctx);
    ActionResult performBattleAction(const std::string& action,
                                     const std::string& target,
                                     GameContext& ctx);

    const BattleState& getBattleState() const;
    bool isInBattle() const;

private:
    std::map<std::string, Enemy> enemies_;
    BattleState battleState_;

    const Enemy* currentEnemy() const;
    ActionResult enemyCounterAttack(GameContext& ctx, bool guarded);
    ActionResult finishVictory(GameContext& ctx, const Enemy& enemy);
    void clearBattle();
};
