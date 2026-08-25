#pragma once

#include <map>
#include <string>

#include "GameContext.h"
#include "NPC.h"

class NPCSystem {
public:
    void initializeNPCs();

    ActionResult talkToNPC(const std::string& npcId, GameContext& ctx);
    bool npcWillHelp(const std::string& npcId, const GameContext& ctx) const;
    std::string getNPCQuest(const std::string& npcId,
                            const GameContext& ctx) const;
    ActionResult completeNPCQuest(const std::string& npcId, GameContext& ctx);

private:
    std::map<std::string, NPC> npcs_;

    const NPC* findNPC(const std::string& npcId) const;
};
