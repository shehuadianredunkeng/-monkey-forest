#pragma once

#include "CommonTypes.h"
#include "Inventory.h"

#include <map>
#include <string>

class Player
{
public:
    int getHealth() const;
    int getStamina() const;
    int getStrength() const;
    int getWisdom() const;
    int getReputation() const;

    void changeHealth(int delta);
    void changeStamina(int delta);
    void changeStrength(int delta);
    void changeWisdom(int delta);
    void changeReputation(int delta);

    int getSkillLevel(SkillType type) const;
    void changeSkillLevel(SkillType type, int delta);

    bool hasItem(const std::string& itemId) const;
    bool addItem(const Item& item);
    bool removeItem(const std::string& itemId);

    const Inventory& getInventory() const;

    const std::string& getCurrentRoomId() const;
    void setCurrentRoomId(const std::string& roomId);

private:
    int health = 100;
    int stamina = 60;
    int strength = 1;
    int wisdom = 1;
    int reputation = 0;

    std::map<SkillType, int> skills = {
        {SkillType::Gather, 1},
        {SkillType::Climb, 1},
        {SkillType::Combat, 1},
        {SkillType::Leadership, 1},
    };

    Inventory inventory;
    std::string currentRoomId = "room_tree";
};
