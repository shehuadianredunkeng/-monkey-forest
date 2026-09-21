#include "UI/GameUI.h"
#include "UI/InteractiveGameUI.h"
#include "UI/TextLayout.h"
#include "CombatSystem.h"
#include "InteractiveMap.h"
#include "Player.h"
#include "Room.h"
#include "TestFramework.h"
#include "WorldState.h"

#include <deque>
#include <iostream>
#include <algorithm>
#include <stdexcept>

using namespace std;

namespace {
struct RecordingSurface final : UI::ConsoleSurface {
    struct Write { int x; int y; wstring text; UI::Color color; };
    vector<Write> writes;
    vector<UI::Rect> clears;
    deque<UI::InputEvent> events;
    int cursorX = 0;
    int cursorY = 0;
    bool visible = false;
    bool fits = true;
    UI::Rect bounds{0, 0, UI::UI_WIDTH, UI::UI_HEIGHT - 1};
    UI::Rect viewport() const override { return bounds; }
    bool prepare() override { return fits; }
    void clear(UI::Rect area) override { clears.push_back(area); }
    void write(UI::SHORT x, UI::SHORT y, const wstring& text, UI::Color color) override {
        writes.push_back({x, y, text, color});
    }
    void cursor(UI::SHORT x, UI::SHORT y, bool show) override {
        cursorX = x; cursorY = y; visible = show;
    }
    int measure(const wstring& glyph) override { return UI::portableColumns(glyph); }
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
    const vector<pair<wstring, vector<wstring>>> cases = {
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
    expect(!UI::fromUtf8(string(1, '\xff')).empty(), "invalid UTF-8 replaced");
    expect(UI::clipText(L"A\tB\x1b", 20, measure).find(L'\x1b') == wstring::npos,
           "escape/control characters must not reach console");
    // 不同宿主对歧义宽度符号的显示可能不一样，这里按实测宽度算。
    auto wideA = [](const wstring& glyph) { return glyph == L"A" ? 2 : UI::portableColumns(glyph); };
    expect(UI::wrapText(L"AAA", 3, wideA) == vector<wstring>({L"A", L"A", L"A"}),
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
    auto device = make_unique<RecordingSurface>();
    auto* output = device.get();
    UI::ConsoleRenderer renderer(move(device));
    UI::GameUI game(renderer);
    auto view = sampleView();
    const vector<wstring> samples = {
        L"纯中文文字", L"English words only", L"中英 mixed 123", L"0123456789",
        L"【】（）！？：，。", L"[]()!?;,:", wstring(300, L'中'),
        L"【智慧 +1】", L"背包：果实 x3\n星猿晶片 x1（关键物品）"
    };
    for (const auto& text : samples) game.appendLog(text);
    view.taskHint = wstring(200, L'任');
    game.render(view);
    expect(!output->clears.empty(), "frame must clear previous content");
    bool success = false;
    bool title = false;
    bool bordersStarted = false;
    vector<bool> divider(UI::INPUT_TOP, false), edge(UI::UI_HEIGHT, false);
    for (const auto& w : output->writes) {
        const bool border = w.text == L"|" || w.text == L"-" || w.text == L"+";
        if (border && (w.x == 0 || w.x == 73 || w.x == 110 || w.y == 0 ||
                       w.y == UI::INPUT_TOP || w.y == UI::UI_HEIGHT - 1 ||
                       (w.x > 73 && (w.y == 8 || w.y == 20)))) {
            bordersStarted = true;
            if (w.x == 73 && w.y < UI::INPUT_TOP) divider[w.y] = true;
            if (w.x == 110) edge[w.y] = true;
            continue;
        }
        expect(!bordersStarted, "borders must be the final drawing pass");
        expect(w.x >= 1 && w.y >= 1 && w.y < UI::UI_HEIGHT - 1, "text outside frame");
        const int end = w.x + UI::displayColumns(w.text, UI::portableColumns);
        if (w.y < UI::INPUT_TOP && w.x < 73) expect(end <= 73, "left text crossed divider");
        else expect(end <= 110, "text crossed right border");
        success = success || (w.text.find(L"智") != wstring::npos && w.color == UI::Color::Success);
        title = title || (w.text.find(L"清") != wstring::npos && w.color == UI::Color::Title);
    }
    for (bool drawn : divider) expect(drawn, "divider is not straight/complete");
    for (bool drawn : edge) expect(drawn, "right border is not straight/complete");
    expect(success, "wisdom reward must be green");
    expect(title, "location title must be cyan");

    output->writes.clear();
    output->clears.clear();
    output->events.push_back({UI::Key::Text, wstring(150, L'中')});
    output->events.push_back({UI::Key::Backspace, {}});
    output->events.push_back({UI::Key::Home, {}});
    output->events.push_back({UI::Key::Text, L"talk "});
    output->events.push_back({UI::Key::End, {}});
    output->events.push_back({UI::Key::Enter, {}});
    auto command = game.readCommand();
    expect(command && *command == L"talk " + wstring(149, L'中'), "long input/edit round trip");
    for (const auto& r : output->clears) expect(r.top == UI::INPUT_ROW && r.bottom == UI::INPUT_ROW,
                                               "typing cleared upper panels");
    for (const auto& w : output->writes) expect(w.y == UI::INPUT_ROW, "typing escaped input row");
    expect(output->cursorX >= 3 && output->cursorX <= 109 && output->cursorY == UI::INPUT_ROW,
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
    expect(view.taskTitle == L"树冠试炼", "objective title adapter");
    expect(view.taskHint.find(L"room_forest") == wstring::npos, "task must hide technical ID");
    expect(view.taskHint.find(L"guide") == wstring::npos,
           "player-facing UI must not require the removed guide command");
    expect(player.getHealth() == 100 && player.getStamina() == 60 && player.getWisdom() == 1 &&
           player.hasItem("item_chip") && world.getTurnCount() == 0 && world.getFlags().size() == 1,
           "UI must not mutate player, turns, flags or inventory");
}

void interactiveSidebarAndChoiceColors() {
    auto device = make_unique<RecordingSurface>();
    auto* output = device.get();
    UI::ConsoleRenderer renderer(move(device));
    UI::InteractiveGameUI ui(renderer);

    Player player;
    player.addItem(Item("item_fruit", "果实", 2));
    player.addItem(Item("item_herb", "草药", 1));
    player.changeSkillLevel(SkillType::Combat, 1);
    player.changeSkillLevel(SkillType::Leadership, 1);
    WorldState world;
    auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    InteractiveMap map;
    map.resetForRoom(ctx);
    CombatSystem combat;
    combat.initializeEnemies();

    ui.appendLog("请选择：\n1. 接受香蕉（力量路线）\n2. 拒绝香蕉（智慧路线）");
    expect(ui.render(ctx, map, combat, "前往果实森林"),
           "interactive UI should render");

    const auto panelRow = [&](int y) {
        vector<RecordingSurface::Write> row;
        for (const auto& write : output->writes)
            if (write.y == y && write.x > UI::DIVIDER_X &&
                write.x < UI::UI_WIDTH)
                row.push_back(write);
        sort(row.begin(), row.end(), [](const auto& left, const auto& right) {
            return left.x < right.x;
        });
        wstring text;
        for (const auto& write : row) text += write.text;
        return text;
    };
    const wstring skills = panelRow(14);
    const wstring inventory = panelRow(17) + panelRow(18);
    const bool skillsVisible =
        skills.find(L"技能") != wstring::npos &&
        skills.find(L"攀爬1") != wstring::npos &&
        skills.find(L"战斗2") != wstring::npos &&
        skills.find(L"领导2") != wstring::npos &&
        skills.find(L"采") == wstring::npos;
    const bool fruitVisible = inventory.find(L"果实x2") != wstring::npos;
    const bool herbVisible = inventory.find(L"草药x1") != wstring::npos;
    bool firstChoiceSeen = false;
    bool secondChoiceSeen = false;
    UI::Color firstChoiceColor = UI::Color::Normal;
    UI::Color secondChoiceColor = UI::Color::Normal;
    for (const auto& write : output->writes) {
        if (write.x < UI::DIVIDER_X && write.text == L"1") {
            firstChoiceSeen = true;
            firstChoiceColor = write.color;
        }
        if (write.x < UI::DIVIDER_X && write.text == L"2") {
            secondChoiceSeen = true;
            secondChoiceColor = write.color;
        }
    }
    expect(skillsVisible, "sidebar must show three skills and remove gathering");
    expect(fruitVisible && herbVisible,
           "inventory item names and counts must remain visible in sidebar");
    expect(firstChoiceSeen && secondChoiceSeen &&
                firstChoiceColor == UI::Color::Hint &&
                secondChoiceColor == firstChoiceColor,
            "all numbered choices must use the same color");

    output->writes.clear();
    output->clears.clear();
    expect(ui.render(ctx, map, combat, "前往果实森林"),
           "stable interactive UI should render again");
    for (const auto& write : output->writes) {
        const bool frameGlyph = write.text == L"|" || write.text == L"-" ||
                                write.text == L"+";
        const bool framePosition = write.x == 0 || write.x == UI::DIVIDER_X ||
            write.x == UI::UI_WIDTH || write.y == 0 || write.y == 18 ||
            write.y == UI::INPUT_TOP || write.y == UI::UI_HEIGHT - 1 ||
            (write.x > UI::DIVIDER_X && (write.y == 8 || write.y == 20));
        expect(!(frameGlyph && framePosition),
               "stable render should not redraw frame/separator borders");
    }
}

void responsiveFrameCases() {
    auto device = make_unique<RecordingSurface>();
    auto* output = device.get();
    UI::ConsoleRenderer renderer(move(device));
    UI::InteractiveGameUI ui(renderer);
    Player player; WorldState world; auto rooms = createAllRooms();
    GameContext ctx{player, world, rooms};
    InteractiveMap map; map.resetForRoom(ctx);
    CombatSystem combat; combat.initializeEnemies();
    for (int i = 0; i < 60; ++i) ui.appendLog("history " + to_string(i));
    for (const auto size : {UI::Rect{0,0,110,33}, UI::Rect{0,0,159,41},
                            UI::Rect{0,0,122,35}, UI::Rect{0,0,110,29}, UI::Rect{0,0,110,33}}) {
        output->bounds = size;
        output->writes.clear();
        expect(ui.render(ctx, map, combat, "目标"), "resize render failed");
        for (int x = 0; x <= size.right; ++x) {
            auto found = find_if(output->writes.rbegin(), output->writes.rend(),
                [&](const auto& w) { return w.x == x && w.y == size.bottom; });
            expect(found != output->writes.rend(), "bottom border missing at resized viewport");
            expect(found->text == (x == 0 || x == size.right ? L"+" : L"-"),
                   "bottom border overwritten by text");
        }
        bool mapLastCell = false;
        bool bottomHint = false;
        for (const auto& w : output->writes) {
            expect(w.x <= size.right && w.y <= size.bottom, "write outside viewport");
            if (w.x == 1 + (map.width()-1)*2 && w.y == 2 + map.height()-1)
                mapLastCell = true;
            if (w.x == 1 && w.y == size.bottom - 3 && w.text == L"W") bottomHint = true;
        }
        expect(mapLastCell, "resize must redraw unchanged map at its original coordinates");
        expect(bottomHint, "controls must follow bottom border");
    }
}

void interactiveNavigationKeys() {
    auto device = make_unique<RecordingSurface>();
    auto* input = device.get();
    UI::ConsoleRenderer renderer(move(device));
    UI::InteractiveGameUI ui(renderer);
    input->events = {
        {UI::Key::Up, {}},
        {UI::Key::Down, {}},
        {UI::Key::Left, {}},
        {UI::Key::Text, L"w"},
        {UI::Key::VirtualText, L"a"},
        {UI::Key::VirtualText, L"1"}
    };
    expect(ui.readExploreAction() == UI::ExploreAction::HistoryUp,
           "up arrow must scroll older story text");
    expect(ui.readExploreAction() == UI::ExploreAction::HistoryDown,
           "down arrow must scroll newer story text");
    expect(ui.readExploreAction() == UI::ExploreAction::None,
           "arrow keys must not move the player");
    expect(ui.readExploreAction() == UI::ExploreAction::MoveUp,
           "WASD must remain the movement control");
    expect(ui.readExploreAction() == UI::ExploreAction::MoveLeft,
           "virtual key fallback must move with a Chinese IME active");
    expect(ui.readExploreAction() == UI::ExploreAction::Choice1,
           "virtual key fallback must still pick numbered options");
}

void rendererRejectsOversizedClipAndTinyWindowInput() {
    auto device = make_unique<RecordingSurface>();
    auto* output = device.get();
    UI::ConsoleRenderer renderer(move(device));
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

void fullScreenPagesCanCrossMainDivider() {
    auto device = make_unique<RecordingSurface>();
    auto* output = device.get();
    UI::ConsoleRenderer renderer(move(device));
    UI::InteractiveGameUI ui(renderer);

    output->events.push_back({UI::Key::Enter, {}});
    ui.showEndingCinematic(L"本轮结局", wstring(80, L'A'));

    bool crossedDivider = false;
    for (const auto& write : output->writes) {
        if (write.y > 0 && write.y < UI::INPUT_TOP && write.x > UI::DIVIDER_X) {
            crossedDivider = true;
            break;
        }
    }
    expect(crossedDivider,
           "ending/full-screen text must not be clipped to the left panel");
}

void collectionPageTogglesConditionsWithYAndN() {
    auto device = make_unique<RecordingSurface>();
    auto* output = device.get();
    UI::ConsoleRenderer renderer(move(device));
    UI::InteractiveGameUI ui(renderer);
    output->events = {
        {UI::Key::Text, L"Y"},
        {UI::Key::Text, L"N"},
        {UI::Key::Enter, {}}
    };
    ui.showCollectionPage(
        L"成就系统",
        L"测试成就\n  评价：这不难，但很好玩。\n？？？",
        L"测试成就  [达成条件：按下Y]\n"
        L"  评价：这不难，但很好玩。\n"
        L"？？？  [达成条件：保持神秘]");

    wstring allDrawnText;
    for (const auto& write : output->writes) {
        allDrawnText += write.text;
    }
    expect(allDrawnText.find(L"达成条件") != wstring::npos,
           "collection page must draw conditions after Y");
    expect(allDrawnText.find(L"Y显示达成条件") != wstring::npos,
           "collection page must initially advertise Y to show conditions");
    expect(allDrawnText.find(L"N隐藏条件") != wstring::npos,
           "collection page must advertise N after conditions are shown");
}

void imeFallbackKeepsCommandsClean() {
    auto device = make_unique<RecordingSurface>();
    auto* input = device.get();
    UI::ConsoleRenderer renderer(move(device));
    UI::InteractiveGameUI ui(renderer);
    input->events = {{UI::Key::VirtualText, L"f"}, {UI::Key::VirtualText, L"e"},
                     {UI::Key::Text, L"蜂蜜"}, {UI::Key::Enter, {}}};
    const auto command = ui.readTypedCommand(L"物品名称 > ");
    expect(command && *command == "蜂蜜",
           "virtual keys must not leak pinyin letters into typed commands");
}

void unicodeEditingAndHistory() {
    auto device = make_unique<RecordingSurface>();
    auto* output = device.get();
    UI::ConsoleRenderer renderer(move(device));
    UI::GameUI game(renderer);
    for (int i = 0; i < 45; ++i) game.appendLog(L"历史记录 " + to_wstring(i));
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
        responsiveFrameCases();
        textCases(); layoutCases(); readOnlyAdapter();
        interactiveSidebarAndChoiceColors();
        interactiveNavigationKeys();
        rendererRejectsOversizedClipAndTinyWindowInput();
        fullScreenPagesCanCrossMainDivider();
        collectionPageTogglesConditionsWithYAndN();
        unicodeEditingAndHistory();
        imeFallbackKeepsCommandsClean();
        cout << "UI tests passed: Unicode, coordinates, colors, input, read-only adapter\n";
    } catch (const exception& error) {
        cerr << error.what() << '\n'; return 1;
    }
}
