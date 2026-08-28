#include "Enemy.h"

#include <algorithm>
#include <utility>

Enemy::Enemy(std::string id, std::string name, std::string description,
             int maxHealth, int attack, int defense, int reputationReward)
    : Character(std::move(id), std::move(name), std::move(description)),
      maxHealth_(std::max(1, maxHealth)),
      attack_(std::max(1, attack)),
      defense_(std::max(0, defense)),
      reputationReward_(std::max(0, reputationReward)) {}

int Enemy::getMaxHealth() const { return maxHealth_; }
int Enemy::getAttack() const { return attack_; }
int Enemy::getDefense() const { return defense_; }
int Enemy::getReputationReward() const { return reputationReward_; }
