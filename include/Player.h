#pragma once

#include "CommonTypes.h"
#include "Inventory.h"

#include <map>
#include <string>

using namespace std;

// Player是保存玩家状态的普通具体类，不是抽象类，也没有派生子类。
// 它把Inventory作为自己的成员，二者是“玩家拥有背包”的组合关系。
class Player
{
public:
    // 下面是一组const成员函数，只读取属性，不会修改Player对象。
    int getHealth() const;
    int getStamina() const;
    int getStrength() const;
    int getWisdom() const;
    int getReputation() const;

    // 下面是状态修改成员函数，通过正负delta增加或减少对应属性。
    void changeHealth(int delta);
    void changeStamina(int delta);
    void changeStrength(int delta);
    void changeWisdom(int delta);
    void changeReputation(int delta);

    // 技能查询是const成员函数，技能修改是普通成员函数。
    int getSkillLevel(SkillType type) const;
    void changeSkillLevel(SkillType type, int delta);

    // 这三个成员函数把物品操作转交给玩家自己的Inventory对象。
    bool hasItem(const string& itemId) const;
    bool addItem(const Item& item);
    bool removeItem(const string& itemId);

    // 返回const引用，界面可以查看背包，但不能绕过Player直接修改。
    const Inventory& getInventory() const;

    // 读取和修改玩家当前所在房间。
    const string& getCurrentRoomId() const;
    void setCurrentRoomId(const string& roomId);

private:
    // 生命、体力和成长属性都由 change 函数统一修改，避免超过游戏范围。
    int health = 100;
    int stamina = 60;
    int strength = 1;
    int wisdom = 1;
    int reputation = 0;

    // 三种技能初始都是1级，战斗技能的上限会高一些。
    map<SkillType, int> skills = {
        {SkillType::Climb, 1},
        {SkillType::Combat, 1},
        {SkillType::Leadership, 1},
    };

    // 玩家自己保存背包和当前所在房间。
    Inventory inventory;
    string currentRoomId = "room_tree";
};
