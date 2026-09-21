#pragma once

#include <map>
#include <string>

#include "CommonTypes.h"
#include "Event.h"

using namespace std;

class WorldState;

class EventSystem {
public:
    void initializeEvents();
    bool canTriggerEvent(const string& eventId,
                         const GameContext& ctx) const;
    ActionResult triggerEvent(const string& eventId,
                              GameContext& ctx);
    ActionResult chooseEventOption(const string& eventId,
                                   int option,
                                   GameContext& ctx);
    ActionResult triggerAvailableMainEvent(GameContext& ctx);
    ActionResult resumePendingEventAfterBattle(GameContext& ctx);
    string getCurrentObjective(const GameContext& ctx) const;
    string getStageIntroduction(int stage) const;
    string getEndingText(const string& endingId) const;

private:
    map<string, Event> events_;
    string activeEventId_;

    const Event* findEvent(const string& eventId) const;
    string findPendingEventId(const WorldState& world) const;
    bool hasOtherPendingEvent(const Event& event,
                              const WorldState& world) const;
    const Event* findRecommendedMainEvent(const GameContext& ctx,
                                          bool requireCurrentRoom) const;
    const Event* chooseRandomEvent(const GameContext& ctx) const;
    ActionResult resolveChoice(const Event& event,
                               int option,
                               GameContext& ctx);
    ActionResult completeEvent(const Event& event,
                               const string& message,
                               GameContext& ctx,
                               bool turnConsumed = true);
    static ActionResult makeResult(bool success,
                                   const string& message,
                                   bool turnConsumed = false,
                                   bool stageCompleted = false);
};
