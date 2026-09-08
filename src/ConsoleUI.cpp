#include "ConsoleUI.h"

#include "CombatSystem.h"
#include "UI/TextLayout.h"

ConsoleUI::ConsoleUI() : gameUI_(renderer_) {}

void ConsoleUI::appendLog(const std::string& utf8Text) {
    gameUI_.appendLog(UI::fromUtf8(utf8Text));
}
void ConsoleUI::render(const GameContext& ctx, const CombatSystem& combat, const std::string& guideText) {
    gameUI_.render(UI::readGameView(ctx, combat.isInBattle(), UI::fromUtf8(guideText)));
}
std::string ConsoleUI::readCommand() {
    const auto command = gameUI_.readCommand();
    inputClosed_ = !command;
    return command ? UI::toUtf8(*command) : std::string{};
}
bool ConsoleUI::inputClosed() const { return inputClosed_; }
void ConsoleUI::restoreCursor() { renderer_.restore(); }
