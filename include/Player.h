#pragma once

#include "CommonTypes.h"

class Player {
public:
    int getStamina() const;
    void changeStamina(int delta);
    int getSkillLevel(SkillType skill) const;
    std::string getCurrentRoomId() const;
    void setCurrentRoomId(const std::string& roomId);
};
