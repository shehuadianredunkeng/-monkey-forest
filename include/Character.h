#pragma once

#include <string>

using namespace std;

class Character {
public:
    Character(string id, string name);
    virtual ~Character() = default;

    const string& getId() const;
    const string& getName() const;

protected:
    string id_;
    string name_;
};
