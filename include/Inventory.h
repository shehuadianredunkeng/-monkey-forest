#pragma once

#include "Item.h"

#include <cstddef>
#include <string>
#include <vector>

using namespace std;

class Inventory
{
public:
    // 相同物品会叠加，所以一个槽位对应一种物品。
    static constexpr size_t MAX_SLOTS = 12;

    bool addItem(const Item& item);

    bool removeItem(const string& itemId);

    bool hasItem(const string& itemId) const;

    const vector<Item>& getItems() const;

private:
    // vector保留物品加入背包时的顺序，显示背包时直接使用。
    vector<Item> items;
};
