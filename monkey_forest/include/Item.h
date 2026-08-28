#pragma once

#include <string>

class Item
{
public:
    Item(std::string id, std::string name, int count = 1);
    Item(std::string id, std::string name, bool important, int count);

    const std::string& getId() const;
    const std::string& getName() const;
    bool isImportant() const;
    int getCount() const;

    void addCount(int delta);
    void reduceCount(int delta);
    void changeCount(int delta);

private:
    std::string id;
    std::string name;
    bool important;
    int count;
};
