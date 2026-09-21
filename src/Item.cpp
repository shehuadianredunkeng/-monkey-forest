#include "Item.h"

#include <algorithm>
#include <utility>

using namespace std;

Item::Item(string id, string name, int count)
    : Item(move(id), move(name), false, count)
{
}

// 上面的三参数构造函数委托给这个完整构造函数，避免重复初始化代码。
Item::Item(string id, string name, bool important, int count)
    : id(move(id)),
      name(move(name)),
      important(important),
      count(count)
{
}

const string& Item::getId() const
{
    return id;
}

const string& Item::getName() const
{
    return name;
}

bool Item::isImportant() const
{
    return important;
}

int Item::getCount() const
{
    return count;
}

void Item::addCount(int delta)
{
    count += delta;
}

void Item::reduceCount(int delta)
{
    // 数量减到0即可，不能出现负数物品。
    count = max(0, count - delta);
}
