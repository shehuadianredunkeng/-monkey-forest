#include "Character.h"
#include <utility>
using namespace std;

//传参 move省掉二次拷贝
Character::Character(string id, string name)
    : id_(move(id)),
      name_(move(name)) {}

const string& Character::getId() const { return id_; }
const string& Character::getName() const { return name_; }
