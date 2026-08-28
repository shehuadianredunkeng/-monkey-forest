#pragma once

#include <map>
#include <string>

#include "CommonTypes.h"
#include "Event.h"

class WorldState;

class EventSystem {
public:
    void initializeEvents();
    bool canTriggerEvent(const std::string& eventId,
                         const GameContext& ctx) const;
    ActionResult triggerEvent(const std::string& eventId,
                              GameContext& ctx);
    ActionResult chooseEventOption(const std::string& eventId,
                                   int option,
                                   GameContext& ctx);
    std::string getStageIntroduction(int stage) const;
    std::string getEndingText(const std::string& endingId) const;

private:
    std::map<std::string, Event> events_;
    std::string activeEventId_;

    const Event* findEvent(const std::string& eventId) const;
    std::string findPendingEventId(const WorldState& world) const;
    bool hasOtherPendingEvent(const Event& event,
                              const WorldState& world) const;
    int countCompletedRandomEvents(const WorldState& world) const;
    const Event* chooseRandomEvent(const GameContext& ctx) const;
    ActionResult resolveChoice(const Event& event,
                               int option,
                               GameContext& ctx);
    ActionResult completeEvent(const Event& event,
                               const std::string& message,
                               GameContext& ctx,
                               bool turnConsumed = true);
    static ActionResult makeResult(bool success,
                                   const std::string& message,
                                   bool turnConsumed = false,
                                   bool stageCompleted = false);
};
