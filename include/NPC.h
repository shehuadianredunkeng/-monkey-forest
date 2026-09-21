#pragma once
#include <utility>
#include "Character.h"
using namespace std;

class NPC : public Character {
public:
    NPC() : Character("", "") {}
    NPC(string id, string name)
        : Character(move(id), move(name)) {}
};//和enemy分开防传错
