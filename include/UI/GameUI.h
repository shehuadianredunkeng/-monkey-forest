#pragma once

#include "UI/ConsoleRenderer.h"

#include <optional>

using namespace std;

struct GameContext;

namespace UI {
struct GameView {
    wstring location;
    wstring taskTitle;
    wstring taskStatus;
    wstring taskHint;
    int health = 100;
    int stamina = 60;
    int wisdom = 1;
    int strength = 1;
    int reputation = 0;
    int inventorySlots = 0;
    bool inBattle = false;
};
GameView readGameView(const GameContext& ctx, bool inBattle,
                      const wstring& objectiveText);

class GameUI {
public:
    explicit GameUI(ConsoleRenderer& renderer);
    void appendLog(const wstring& text);
    bool render(const GameView& view);
    optional<wstring> readCommand();
private:
    struct Line { wstring text; Color color; };
    ConsoleRenderer& renderer_;
    vector<Line> history_;
    GameView lastView_;
    size_t scrollBack_ = 0;
    bool frameReady_ = false;
    void paragraph(Rect area, const wstring& text, Color color);
    void bar(SHORT y, int value);
    void drawInput(const vector<wstring>& glyphs, size_t caret);
};
}
