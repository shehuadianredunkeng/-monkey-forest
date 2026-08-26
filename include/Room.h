#pragma once

#include "CommonTypes.h"

#include <map>
#include <string>
#include <vector>

class Room {
public:
    Room() = default;
    Room(std::string id,
         std::string name,
         std::string baseDescription,
         std::map<std::string, std::string> exits,
         std::vector<std::string> npcIds,
         std::vector<std::string> itemIds,
         std::string recommendedAction);

    const std::string& getId() const;
    const std::string& getName() const;
    const std::string& getBaseDescription() const;
    const std::map<std::string, std::string>& getExits() const;
    const std::vector<std::string>& getNPCIds() const;
    const std::vector<std::string>& getItemIds() const;
    const std::string& getRecommendedAction() const;

private:
    std::string id_;
    std::string name_;
    std::string baseDescription_;
    std::map<std::string, std::string> exits_;
    std::vector<std::string> npcIds_;
    std::vector<std::string> itemIds_;
    std::string recommendedAction_;
};

std::map<std::string, Room> createAllRooms();
ActionResult movePlayer(GameContext& context, const std::string& direction);
std::string lookAround(const GameContext& context);
std::string getCommandHelp();
