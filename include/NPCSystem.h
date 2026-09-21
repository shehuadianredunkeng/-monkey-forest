#pragma once
#include <map>
#include <string>
#include "CommonTypes.h"
#include "NPC.h"
using namespace std;

class NPCSystem {
public:
    void initializeNPCs();
    ActionResult talkToNPC(const string& npcId, GameContext& ctx);
    ActionResult chooseNPCDialogue(const string& npcId,
                                   int option,
                                   GameContext& ctx);
    ActionResult chooseDialogueOption(int option, GameContext& ctx);
    bool npcWillHelp(const string& npcId, const GameContext& ctx) const;//用来判定闪尾的
    string getNPCQuest(const string& npcId,
                            const GameContext& ctx) const;
    ActionResult completeNPCQuest(const string& npcId, GameContext& ctx);

private:
    map<string, NPC> npcs_;
    string activeDialogueNpcId_;

    const NPC* findNPC(const string& npcId) const;//判定npc位置范围
    string normalizeNPCId(const string& npcId) const;//统一id
};
