#include "Character.h"

#include <utility>

using namespace std;

Character::Character(string id, string name)
    : id_(move(id)),
      name_(move(name)) {}

const string& Character::getId() const { return id_; }
const string& Character::getName() const { return name_; }
