#pragma once

#include "UI/GameUI.h"

#include <string>

class CombatSystem;
struct GameContext;

class ConsoleUI {
public:
    ConsoleUI();
    void appendLog(const std::string& utf8Text);
    void render(const GameContext& ctx,
                const CombatSystem& combat,
                const std::string& guideText);
    std::string readCommand();
    bool inputClosed() const;
    void restoreCursor();

private:
    UI::ConsoleRenderer renderer_;
    UI::GameUI gameUI_;
    bool inputClosed_ = false;
};
