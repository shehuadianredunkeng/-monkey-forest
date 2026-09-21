#include "Enemy.h"

#include <algorithm>
#include <utility>

using namespace std;

Enemy::Enemy(string id, string name,
             int maxHealth, int attack, int defense, int reputationReward)
    : Character(move(id), move(name)),
      maxHealth_(max(1, maxHealth)),
      attack_(max(1, attack)),
      defense_(max(0, defense)),
      reputationReward_(max(0, reputationReward)) {}

int Enemy::getMaxHealth() const { return maxHealth_; }
int Enemy::getAttack() const { return attack_; }
int Enemy::getDefense() const { return defense_; }
int Enemy::getReputationReward() const { return reputationReward_; }
