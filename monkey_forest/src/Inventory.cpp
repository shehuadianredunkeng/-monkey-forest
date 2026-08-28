#include "Inventory.h"

#include <algorithm>

bool Inventory::addItem(const Item& item)
{
    if (item.getCount() <= 0)
    {
        return false;
    }

    auto existing = std::find_if(
        items.begin(),
        items.end(),
        [&item](const Item& stored) { return stored.getId() == item.getId(); });

    if (existing != items.end())
    {
        existing->addCount(item.getCount());
        return true;
    }

    if (isFull())
    {
        return false;
    }

    items.push_back(item);
    return true;
}

bool Inventory::removeItem(const std::string& itemId)
{
    auto existing = std::find_if(
        items.begin(),
        items.end(),
        [&itemId](const Item& stored) { return stored.getId() == itemId; });

    if (existing == items.end() || existing->isImportant())
    {
        return false;
    }

    existing->reduceCount(1);
    if (existing->getCount() == 0)
    {
        items.erase(existing);
    }
    return true;
}

bool Inventory::hasItem(const std::string& itemId) const
{
    return std::any_of(
        items.cbegin(),
        items.cend(),
        [&itemId](const Item& stored) { return stored.getId() == itemId; });
}

const std::vector<Item>& Inventory::getItems() const
{
    return items;
}

bool Inventory::isFull() const
{
    return items.size() >= MAX_SLOTS;
}
