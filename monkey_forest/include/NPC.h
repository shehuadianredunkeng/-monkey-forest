#pragma once

#include <utility>

#include "Character.h"

class NPC : public Character {
public:
    NPC() : Character("", "", "") {}
    NPC(std::string id, std::string name, std::string description)
        : Character(std::move(id), std::move(name), std::move(description)) {}
};
