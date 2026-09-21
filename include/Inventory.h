#pragma once

#include "Item.h"

#include <cstddef>
#include <string>
#include <vector>

using namespace std;

class Inventory
{
public:
    static constexpr size_t MAX_SLOTS = 12;

    bool addItem(const Item& item);

    bool removeItem(const string& itemId);

    bool hasItem(const string& itemId) const;

    const vector<Item>& getItems() const;

private:
    vector<Item> items;
};
