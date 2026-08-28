#pragma once

#include <string>

class Character {
public:
    Character(std::string id, std::string name, std::string description);
    virtual ~Character() = default;

    const std::string& getId() const;
    const std::string& getName() const;
    const std::string& getDescription() const;

protected:
    std::string id_;
    std::string name_;
    std::string description_;
};
