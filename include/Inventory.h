#pragma once

#include "Item.h"

#include <cstddef>
#include <string>
#include <vector>

using namespace std;

// Inventory是管理物品的普通容器类，不是抽象类，也没有子类。
// 它内部拥有多个Item对象，和Item之间是组合关系，不是继承关系。
class Inventory
{
public:
    // 相同物品会叠加，所以一个槽位对应一种物品。
    static constexpr size_t MAX_SLOTS = 12;

    // 普通成员函数：加入物品，成功返回true，背包满时返回false。
    bool addItem(const Item& item);

    // 普通成员函数：每次删除一个普通物品，关键物品不能删除。
    bool removeItem(const string& itemId);

    // const成员函数：只检查物品是否存在，不改变背包内容。
    bool hasItem(const string& itemId) const;

    // const成员函数：把物品列表以只读引用交给显示和查询代码。
    const vector<Item>& getItems() const;

private:
    // vector保留物品加入背包时的顺序，显示背包时直接使用。
    vector<Item> items;
};
