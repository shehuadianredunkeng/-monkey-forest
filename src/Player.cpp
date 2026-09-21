#include "Player.h"

#include <algorithm>

using namespace std;

namespace
{
// 文件内部辅助函数，不属于Player类，只在本文件中使用。
// 玩家数值统一在这里限制上下界。
int changeValue(int value, int delta, int minimum, int maximum)
{
    return clamp(value + delta, minimum, maximum);
}
} // namespace

int Player::getHealth() const
{
    return health;
}

int Player::getStamina() const
{
    return stamina;
}

int Player::getStrength() const
{
    return strength;
}

int Player::getWisdom() const
{
    return wisdom;
}

int Player::getReputation() const
{
    return reputation;
}

void Player::changeHealth(int delta)
{
    health = changeValue(health, delta, 0, 100);
}

void Player::changeStamina(int delta)
{
    stamina = changeValue(stamina, delta, 0, 100);
}

void Player::changeStrength(int delta)
{
    strength = changeValue(strength, delta, 1, 5);
}

void Player::changeWisdom(int delta)
{
    wisdom = changeValue(wisdom, delta, 1, 5);
}

void Player::changeReputation(int delta)
{
    reputation = changeValue(reputation, delta, 0, 100);
}

int Player::getSkillLevel(SkillType type) const
{
    return skills.at(type);
}

void Player::changeSkillLevel(SkillType type, int delta)
{
    // 战斗内容较多，战斗技能最高5级，另外两种最高3级。
    const int maximum = type == SkillType::Combat ? 5 : 3;
    skills[type] = changeValue(getSkillLevel(type), delta, 1, maximum);
}

bool Player::hasItem(const string& itemId) const
{
    return inventory.hasItem(itemId);
}

bool Player::addItem(const Item& item)
{
    return inventory.addItem(item);
}

bool Player::removeItem(const string& itemId)
{
    return inventory.removeItem(itemId);
}

const Inventory& Player::getInventory() const
{
    return inventory;
}

const string& Player::getCurrentRoomId() const
{
    return currentRoomId;
}

void Player::setCurrentRoomId(const string& roomId)
{
    currentRoomId = roomId;
}
