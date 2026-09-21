#pragma once

#include "UI/GameUI.h"

#include <string>

using namespace std;

class CombatSystem;
struct GameContext;

class ConsoleUI {
public:
    ConsoleUI();
    void appendLog(const string& utf8Text);
    void render(const GameContext& ctx,
                const CombatSystem& combat,
                const string& objectiveText);
    string readCommand();
    void restoreCursor();

private:
    UI::ConsoleRenderer renderer_;
    UI::GameUI gameUI_;
};
