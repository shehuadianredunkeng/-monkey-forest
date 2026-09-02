#pragma once

#include "CommonTypes.h"
#include "NPC.h"

#include <map>
#include <string>

class NPCSystem {
public:
    void initializeNPCs();

    ActionResult talkToNPC(const std::string& npcId, GameContext& ctx);

    ActionResult chooseNPCDialogue(const std::string& npcId,
                                   int option,
                                   GameContext& ctx);

    ActionResult chooseDialogueOption(int option, GameContext& ctx);

    bool npcWillHelp(const std::string& npcId,
                     const GameContext& ctx) const;

    std::string getNPCQuest(const std::string& npcId,
                            const GameContext& ctx) const;

    ActionResult completeNPCQuest(const std::string& npcId,
                                  GameContext& ctx);

private:
    const NPC* findNPC(const std::string& npcId) const;

    std::string normalizeNPCId(const std::string& npcId) const;

    std::map<std::string, NPC> npcs_;
    std::string activeDialogueNpcId_;
};
