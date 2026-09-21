#pragma once

#include <string>

using namespace std;

class Item
{
public:
    Item(string id, string name, int count = 1);
    Item(string id, string name, bool important, int count);

    const string& getId() const;
    const string& getName() const;
    bool isImportant() const;
    int getCount() const;

    void addCount(int delta);
    void reduceCount(int delta);

private:
    string id;
    string name;
    bool important;
    int count;
};
