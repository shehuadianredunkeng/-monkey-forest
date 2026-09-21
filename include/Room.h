#pragma once

#include "CommonTypes.h"

#include <map>
#include <string>
#include <vector>

using namespace std;

class Room {
public:
    Room() = default;
    Room(string id,
         string name,
         string baseDescription,
         map<string, string> exits,
         vector<string> npcIds,
         vector<string> itemIds);

    const string& getId() const;
    const string& getName() const;
    const string& getBaseDescription() const;
    const map<string, string>& getExits() const;
    const vector<string>& getNPCIds() const;
    const vector<string>& getItemIds() const;

    vector<string> getVisibleNPCIds(const GameContext& context) const;
    vector<string> getVisibleItemIds(const GameContext& context) const;
    string getDynamicRecommendation(const GameContext& context) const;

private:
    string id_;
    string name_;
    string baseDescription_;
    map<string, string> exits_;
    vector<string> npcIds_;
    vector<string> itemIds_;
};

map<string, Room> createAllRooms();
ActionResult movePlayer(GameContext& context, const string& direction);
string lookAround(const GameContext& context);
string showMap(const GameContext& context);
string getCommandHelp();
