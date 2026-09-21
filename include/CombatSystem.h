#pragma once
//防止重复定义
#include <map>
#include <string>
#include "Enemy.h"
#include "CommonTypes.h"
using namespace std;

struct BattleState {
    bool inBattle = false;
    string enemyId;
    string encounterId;
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
    CombatSystem();

    ActionResult startBattle(const string& enemyId, GameContext& ctx);
    ActionResult performBattleAction(const string& action,
                                     const string& target,
                                     GameContext& ctx);
    ActionResult chooseEscapeEndingOption(int option, GameContext& ctx);

    const BattleState& getBattleState() const;
    bool isInBattle() const;
    void saveBattleState(WorldState& world) const;
    bool restoreBattleState(GameContext& ctx);
    void clearSavedBattleState(WorldState& world) const;//清除战斗中写入的存档旗标

private:
    void initializeEnemies();

    map<string, Enemy> enemies_;
    BattleState battleState_;
    int battleTurn_ = 0;
    bool enemyArmorActive_ = false;

    const Enemy* currentEnemy() const;
    ActionResult enemyCounterAttack(GameContext& ctx, bool guarded);
    ActionResult finishVictory(GameContext& ctx, const Enemy& enemy);
    ActionResult handleBananaChoice(const string& target,
                                    GameContext& ctx);
    ActionResult handleTheft(GameContext& ctx);
    ActionResult handleFlintAttack(GameContext& ctx);
    void recordTheftAchievement(GameContext& ctx);
    int playerAttackDamage(const Enemy& enemy, const GameContext& ctx) const;
    void clearBattle();
};
