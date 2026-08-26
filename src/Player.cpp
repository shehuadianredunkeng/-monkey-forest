#include "Player.h"

#include <algorithm>

namespace
{
int changedAndClamped(int value, int delta, int minimum, int maximum)
{
    const long long changed = static_cast<long long>(value) + delta;
    return static_cast<int>(std::clamp(
        changed,
        static_cast<long long>(minimum),
        static_cast<long long>(maximum)));
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
    health = changedAndClamped(health, delta, 0, 100);
}

void Player::changeStamina(int delta)
{
    stamina = changedAndClamped(stamina, delta, 0, 100);
}

void Player::changeStrength(int delta)
{
    strength = changedAndClamped(strength, delta, 1, 5);
}

void Player::changeWisdom(int delta)
{
    wisdom = changedAndClamped(wisdom, delta, 1, 5);
}

void Player::changeReputation(int delta)
{
    reputation = changedAndClamped(reputation, delta, 0, 100);
}

int Player::getSkillLevel(SkillType type) const
{
    const auto skill = skills.find(type);
    return skill == skills.end() ? 1 : skill->second;
}

void Player::changeSkillLevel(SkillType type, int delta)
{
    skills[type] = changedAndClamped(getSkillLevel(type), delta, 1, 3);
}

bool Player::hasItem(const std::string& itemId) const
{
    return inventory.hasItem(itemId);
}

bool Player::addItem(const Item& item)
{
    return inventory.addItem(item);
}

bool Player::removeItem(const std::string& itemId)
{
    return inventory.removeItem(itemId);
}

const Inventory& Player::getInventory() const
{
    return inventory;
}

const std::string& Player::getCurrentRoomId() const
{
    return currentRoomId;
}

void Player::setCurrentRoomId(const std::string& roomId)
{
    currentRoomId = roomId;
}
