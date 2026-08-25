#include "Character.h"

#include <utility>

Character::Character(std::string id, std::string name, std::string description)
    : id_(std::move(id)),
      name_(std::move(name)),
      description_(std::move(description)) {}

const std::string& Character::getId() const { return id_; }
const std::string& Character::getName() const { return name_; }
const std::string& Character::getDescription() const { return description_; }
