#include "UI/GameUI.h"
#include "UI/TextLayout.h"
#include "Player.h"
#include "Room.h"
#include "WorldState.h"

#include <deque>
#include <iostream>
#include <stdexcept>

namespace {
void expect(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

// Only replace the OS device. Layout, wrapping, editor and model adapter are real.
struct RecordingSurface final : UI::ConsoleSurface {
    struct Write { int x; int y; std::wstring text; UI::Color color; };
    std::vector<Write> writes;
    std::vector<UI::Rect> clears;
    std::deque<UI::InputEvent> events;
    int cursorX = 0;
    int cursorY = 0;
    bool visible = false;
    bool fits = true;
    bool prepare() override { return fits; }
    void clear(UI::Rect area) override { clears.push_back(area); }
    void write(UI::SHORT x, UI::SHORT y, const std::wstring& text, UI::Color color) override {
        writes.push_back({x, y, text, color});
    }
    void cursor(UI::SHORT x, UI::SHORT y, bool show) override {
        cursorX = x; cursorY = y; visible = show;
    }
    int measure(const std::wstring& glyph) override { return UI::portableColumns(glyph); }
    UI::InputEvent readEvent() override {
        if (events.empty()) return {UI::Key::EndOfInput, {}};
        const auto event = events.front(); events.pop_front();
        if (event.key == UI::Key::Resize) fits = true;
        return event;
    }
    void restore() override {}
};

void textCases() {
    const auto measure = UI::portableColumns;
    const std::vector<std::pair<std::wstring, std::vector<std::wstring>>> cases = {
        {L"中文测试", {L"中文", L"测试"}},
        {L"abcdef", {L"abcd", L"ef"}},
        {L"中A文B", {L"中A", L"文B"}},
        {L"123456", {L"1234", L"56"}},
        {L"，。！？", {L"，。", L"！？"}},
        {L",.!?()", {L",.!?", L"()"}},
        {L"A\r\nB", {L"A", L"B"}},
        {L"e\u0301中文", {L"e\u0301中", L"文"}},
        {L"\U0001f600中文", {L"\U0001f600中", L"文"}}
    };
    for (const auto& c : cases)
        expect(UI::wrapText(c.first, 4, measure) == c.second, "cell wrapping mismatch");
    expect(UI::clipText(L"中A", 1, measure).empty(), "must not split a wide glyph");
    expect(UI::wrapText(L"中文", 0, measure).empty(), "zero width must terminate");
    expect(UI::displayColumns(L"【智慧 +1】", measure) == 11, "reward cell width");
    expect(UI::toUtf8(UI::fromUtf8("中A😀")) == "中A😀", "UTF-8 round trip");
    expect(!UI::fromUtf8(std::string(1, '\xff')).empty(), "invalid UTF-8 replaced");
    expect(UI::clipText(L"A\tB\x1b", 20, measure).find(L'\x1b') == std::wstring::npos,
           "escape/control characters must not reach console");
    // A host may display ambiguous symbols differently. Use its measured width.
    auto wideA = [](const std::wstring& glyph) { return glyph == L"A" ? 2 : UI::portableColumns(glyph); };
    expect(UI::wrapText(L"AAA", 3, wideA) == std::vector<std::wstring>({L"A", L"A", L"A"}),
           "must use device measurement instead of string size");
}

UI::GameView sampleView() {
    UI::GameView view;
    view.location = L"清泉河谷 River Valley";
    view.taskTitle = L"寻找豆豆";
    view.taskStatus = L"进行中";
    view.taskHint = L"输入 talk 豆豆";
    view.health = 80; view.stamina = 60; view.wisdom = 3; view.strength = 2;
    return view;
}

void layoutCases() {
    auto device = std::make_unique<RecordingSurface>();
    auto* output = device.get();
    UI::ConsoleRenderer renderer(std::move(device));
    UI::GameUI game(renderer);
    auto view = sampleView();
    const std::vector<std::wstring> samples = {
        L"纯中文文字", L"English words only", L"中英 mixed 123", L"0123456789",
        L"【】（）！？：，。", L"[]()!?;,:", std::wstring(300, L'中'),
        L"【智慧 +1】", L"背包：果实 x3\n星猿晶片 x1（关键物品）"
    };
    for (const auto& text : samples) game.appendLog(text);
    view.taskHint = std::wstring(200, L'任');
    game.render(view);
    expect(!output->clears.empty(), "frame must clear previous content");
    bool success = false;
    bool title = false;
    bool bordersStarted = false;
    std::vector<bool> divider(25, false), edge(30, false);
    for (const auto& w : output->writes) {
        const bool border = w.text == L"|" || w.text == L"-" || w.text == L"+";
        if (border && (w.x == 0 || w.x == 73 || w.x == 110 || w.y == 0 || w.y == 25 || w.y == 29 ||
                       (w.x > 73 && (w.y == 8 || w.y == 20)))) {
            bordersStarted = true;
            if (w.x == 73 && w.y < 25) divider[w.y] = true;
            if (w.x == 110) edge[w.y] = true;
            continue;
        }
        expect(!bordersStarted, "borders must be the final drawing pass");
        expect(w.x >= 1 && w.y >= 1 && w.y < 29, "text outside frame");
        const int end = w.x + UI::displayColumns(w.text, UI::portableColumns);
        if (w.y < 25 && w.x < 73) expect(end <= 73, "left text crossed divider");
        else expect(end <= 110, "text crossed right border");
        success = success || (w.text.find(L"智") != std::wstring::npos && w.color == UI::Color::Success);
        title = title || (w.text.find(L"清") != std::wstring::npos && w.color == UI::Color::Title);
    }
    for (bool drawn : divider) expect(drawn, "divider is not straight/complete");
    for (bool drawn : edge) expect(drawn, "right border is not straight/complete");
    expect(success, "wisdom reward must be green");
    expect(title, "location title must be cyan");

    output->writes.clear();
    output->clears.clear();
    output->events.push_back({UI::Key::Text, std::wstring(150, L'中')});
    output->events.push_back({UI::Key::Backspace, {}});
    output->events.push_back({UI::Key::Home, {}});
    output->events.push_back({UI::Key::Text, L"talk "});
    output->events.push_back({UI::Key::End, {}});
    output->events.push_back({UI::Key::Enter, {}});
    auto command = game.readCommand();
    expect(command && *command == L"talk " + std::wstring(149, L'中'), "long input/edit round trip");
    for (const auto& r : output->clears) expect(r.top == 27 && r.bottom == 27, "typing cleared upper panels");
    for (const auto& w : output->writes) expect(w.y == 27, "typing escaped input row");
    expect(output->cursorX >= 3 && output->cursorX <= 109 && output->cursorY == 27,
           "input cursor crossed frame");
    expect(!game.readCommand(), "EOF must terminate input");
}

void readOnlyAdapter() {
    Player player;
    player.addItem(Item("item_chip", "星猿晶片", true));
    WorldState world;
    world.setFlag("flag_keep");
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    const auto view = UI::readGameView(ctx, false, L"当前主线：树冠试炼\n目标地点：果实森林（room_forest）\n输入 investigate");
    expect(view.health == 100 && view.stamina == 60 && view.wisdom == 1, "existing getter adapter");
    expect(view.inventorySlots == 1, "inventory count must use public getter");
    expect(view.taskTitle == L"树冠试炼", "guide title adapter");
    expect(view.taskHint.find(L"room_forest") == std::wstring::npos, "task must hide technical ID");
    expect(player.getHealth() == 100 && player.getStamina() == 60 && player.getWisdom() == 1 &&
           player.hasItem("item_chip") && world.getTurnCount() == 0 && world.getFlags().size() == 1,
           "UI must not mutate player, turns, flags or inventory");
}

void rendererRejectsOversizedClipAndTinyWindowInput() {
    auto device = std::make_unique<RecordingSurface>();
    auto* output = device.get();
    UI::ConsoleRenderer renderer(std::move(device));
    renderer.beginFrame();
    renderer.drawTextIn({0, 0, 200, 50}, 72, 3, L"中文越界", UI::Color::Normal);
    expect(output->writes.empty(), "even an oversized clip rectangle must protect divider");
    renderer.drawTextIn({0, 0, 200, 50}, 74, 8, L"不应覆盖横线", UI::Color::Normal);
    expect(output->writes.empty(), "panel separator must be protected");
    UI::GameUI game(renderer);
    output->fits = false;
    expect(!game.render(sampleView()), "small window must reject normal frame");
    output->events = {{UI::Key::Text, L"take"}, {UI::Key::Enter, {}},
                      {UI::Key::Resize, {}}, {UI::Key::Text, L"look"}, {UI::Key::Enter, {}}};
    const auto command = game.readCommand();
    expect(command && *command == L"look", "invisible UI must not accept gameplay commands");
}

void unicodeEditingAndHistory() {
    auto device = std::make_unique<RecordingSurface>();
    auto* output = device.get();
    UI::ConsoleRenderer renderer(std::move(device));
    UI::GameUI game(renderer);
    for (int i = 0; i < 45; ++i) game.appendLog(L"历史记录 " + std::to_wstring(i));
    game.render(sampleView());
    output->events = {{UI::Key::Text, L"e"}, {UI::Key::Text, L"\u0301"},
                      {UI::Key::Backspace, {}}, {UI::Key::Text, L"中\U0001f600B"},
                      {UI::Key::Left, {}}, {UI::Key::Delete, {}}, {UI::Key::Left, {}},
                      {UI::Key::Text, L"A"}, {UI::Key::PageUp, {}}, {UI::Key::PageDown, {}},
                      {UI::Key::Enter, {}}};
    const auto command = game.readCommand();
    expect(command && *command == L"中A\U0001f600", "editing must preserve Unicode glyphs and typed input during paging");
}
}

int main() {
    try {
        textCases(); layoutCases(); readOnlyAdapter();
        rendererRejectsOversizedClipAndTinyWindowInput(); unicodeEditingAndHistory();
        std::cout << "UI tests passed: Unicode, coordinates, colors, input, read-only adapter\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
