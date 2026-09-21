#pragma once

#include "InteractiveMap.h"
#include "UI/ConsoleRenderer.h"

#include <optional>
#include <map>
#include <string>
#include <vector>

using namespace std;

class CombatSystem;
struct GameContext;

namespace UI {

enum class ExploreAction {
    None,
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Interact,
    Inventory,
    UseItem,
    Save,
    Help,
    Menu,
    Choice1,
    Choice2,
    Choice3,
    Choice4,
    HistoryUp,
    HistoryDown,
    EndOfInput
};

class InteractiveGameUI {
public:
    explicit InteractiveGameUI(ConsoleRenderer& renderer);

    void appendLog(const string& text);
    bool render(const GameContext& ctx,
                const InteractiveMap& map,
                const CombatSystem& combat,
                const string& objective);
    ExploreAction readExploreAction();
    optional<string> readTypedCommand(const wstring& prompt,
                                               bool battleSaveShortcut = false);

    int showMainMenu(bool hasAnySave, bool preferContinue = false);
    int showPauseMenu();
    int showSlotMenu(const wstring& title,
                     const vector<wstring>& slotDescriptions,
                     bool allowEmpty,
                     int preferredSlot = 0);
    void showTextPage(const wstring& title, const wstring& text);
    void showCollectionPage(const wstring& title,
                            const wstring& summaryText,
                            const wstring& conditionsText);
    void showEndingCinematic(const wstring& title,
                             const wstring& text);
    void clearLog();

private:
    struct LogLine {
        wstring text;
        Color color = Color::Normal;
    };

    ConsoleRenderer& renderer_;
    vector<LogLine> history_;
    vector<MapTileVisual> lastMapTiles_;
    map<int, LogLine> lastStableRows_;
    size_t historyScrollBack_ = 0;
    bool needsFullClear_ = true;

    void centered(SHORT y, const wstring& text, Color color);
    void drawStableLine(Rect area, SHORT y, const wstring& text,
                        Color color);
    int menu(const wstring& title,
             const vector<wstring>& options,
             int initial = 0);
    void drawTypedInput(const wstring& prompt,
                        const vector<wstring>& glyphs,
                        size_t caret);
};

}  // namespace UI
