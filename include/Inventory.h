#pragma once

#include "Item.h"

#include <cstddef>
#include <string>
#include <vector>

class Inventory
{
public:
    bool addItem(const Item& item);

    bool removeItem(const std::string& itemId);

    bool hasItem(const std::string& itemId) const;

    const std::vector<Item>& getItems() const;

    bool isFull() const;

private:
    static constexpr std::size_t MAX_SLOTS = 8;
    std::vector<Item> items;
};
