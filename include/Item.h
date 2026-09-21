#pragma once

#include <string>

using namespace std;

// Item是表示一类物品的普通数据类，不是抽象类，也没有子类。
// 同一ID的多件物品用count累计，不需要为每一件都建立对象。
class Item
{
public:
    // 两个重载构造函数：普通物品可省略important，剧情物品要明确传入。
    Item(string id, string name, int count = 1);
    Item(string id, string name, bool important, int count);

    // const成员函数，只读取物品信息。
    const string& getId() const;
    const string& getName() const;
    bool isImportant() const;
    int getCount() const;

    // 普通成员函数，修改这一叠物品的数量。
    void addCount(int delta);
    void reduceCount(int delta);

private:
    string id;
    string name;
    // 剧情物品不能像普通消耗品一样从背包删除。
    bool important;
    int count;
};
