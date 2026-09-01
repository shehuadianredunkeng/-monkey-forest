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
    bool awaitingBananaChoice = false;
    bool bananaGreedLoop = false;
    bool theftUsed = false;
    bool fireUsed = false;
    int consecutiveGuards = 0;
    int doubleDamageTurns = 0;
};

class CombatSystem {
public:
    void initializeEnemies();

    ActionResult startBattle(const std::string& enemyId, GameContext& ctx);
    ActionResult performBattleAction(const std::string& action,
                                     const std::string& target,
                                     GameContext& ctx);
    ActionResult chooseEscapeEndingOption(int option, GameContext& ctx);

    const BattleState& getBattleState() const;
    bool isInBattle() const;

private:
    std::map<std::string, Enemy> enemies_;
    BattleState battleState_;
    int battleTurn_ = 0;
    bool enemyArmorActive_ = false;

    const Enemy* currentEnemy() const;
    ActionResult enemyCounterAttack(GameContext& ctx, bool guarded);
    ActionResult finishVictory(GameContext& ctx, const Enemy& enemy);
    ActionResult handleBananaChoice(const std::string& target,
                                    GameContext& ctx);
    ActionResult handleTheft(GameContext& ctx);
    ActionResult handleFlintAttack(GameContext& ctx);
    void recordTheftAchievement(GameContext& ctx);
    int playerAttackDamage(const Enemy& enemy, const GameContext& ctx) const;
    void clearBattle();
};
