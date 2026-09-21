#include "Inventory.h"

#include <algorithm>

using namespace std;

bool Inventory::addItem(const Item& item)
{
    if (item.getCount() <= 0)
    {
        return false;
    }

    auto existing = find_if(
        items.begin(),
        items.end(),
        [&item](const Item& stored) { return stored.getId() == item.getId(); });

    // 背包已满时也可以继续叠加已有物品。
    if (existing != items.end())
    {
        existing->addCount(item.getCount());
        return true;
    }

    // 只有加入新品种时才会占用新槽位。
    if (items.size() >= MAX_SLOTS)
    {
        return false;
    }

    items.push_back(item);
    return true;
}

bool Inventory::removeItem(const string& itemId)
{
    auto existing = find_if(
        items.begin(),
        items.end(),
        [&itemId](const Item& stored) { return stored.getId() == itemId; });

    // 没找到物品或物品属于剧情道具时都不能删除。
    if (existing == items.end() || existing->isImportant())
    {
        return false;
    }

    existing->reduceCount(1);
    // 最后一件被用掉后，这个槽位也一起清除。
    if (existing->getCount() == 0)
    {
        items.erase(existing);
    }
    return true;
}

bool Inventory::hasItem(const string& itemId) const
{
    return any_of(
        items.cbegin(),
        items.cend(),
        [&itemId](const Item& stored) { return stored.getId() == itemId; });
}

const vector<Item>& Inventory::getItems() const
{
    return items;
}
