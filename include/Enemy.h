#pragma once

#include "Character.h"

class Enemy : public Character {
public:
    Enemy() : Character("", "", "") {}
    Enemy(std::string id, std::string name, std::string description,
          int maxHealth, int attack, int defense, int reputationReward);

    int getMaxHealth() const;
    int getAttack() const;
    int getDefense() const;
    int getReputationReward() const;

private:
    int maxHealth_ = 1;
    int attack_ = 1;
    int defense_ = 0;
    int reputationReward_ = 0;
};
