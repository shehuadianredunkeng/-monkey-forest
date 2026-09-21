#include <windows.h>
#include "UI/InteractiveGameUI.h"
#include "Player.h"
#include "Room.h"
#include "TestFramework.h"
#include "WorldState.h"
#include "CombatSystem.h"
#include <fstream>
#include <stdexcept>

using namespace std;

int main(int argc, char** argv)
{
    ofstream report(argc > 1 ? argv[1] : "native_resize_report.txt");
    try
    {
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode;
        expect(GetConsoleMode(out, &mode), "test needs its own console");

        CONSOLE_FONT_INFOEX font{};
        font.cbSize = sizeof(font);
        if (GetCurrentConsoleFontEx(out, FALSE, &font))
        {
            font.dwFontSize = {8, 16};
            SetCurrentConsoleFontEx(out, FALSE, &font);
        }

        SMALL_RECT tiny{0, 0, 79, 24};
        expect(SetConsoleWindowInfo(out, TRUE, &tiny), "set initial viewport");
        expect(SetConsoleScreenBufferSize(out, {180, 80}), "set test buffer");

        UI::ConsoleRenderer renderer;
        UI::InteractiveGameUI ui(renderer);
        Player player;
        WorldState world;
        auto rooms = createAllRooms();
        GameContext ctx{player, world, rooms};
        InteractiveMap map;
        map.resetForRoom(ctx);
        CombatSystem combat;
        for (int i = 0; i < 60; ++i)
            ui.appendLog("history " + to_string(i));

        // 放大、缩小，再回到标准尺寸，确认每次都能把边框完整重画。
        const COORD sizes[] = {{111, 34}, {160, 42}, {123, 36},
                               {111, 30}, {111, 34}};
        for (COORD size : sizes)
        {
            SMALL_RECT window{0, 0, SHORT(size.X - 1), SHORT(size.Y - 1)};
            expect(SetConsoleWindowInfo(out, TRUE, &window), "resize native viewport");
            expect(ui.render(ctx, map, combat, "前往果实森林"), "native render failed");

            for (SHORT y : {SHORT(0), SHORT(size.Y - 5), SHORT(size.Y - 1)})
            {
                for (SHORT x = 0; x < size.X; ++x)
                {
                    wchar_t ch = 0;
                    DWORD n = 0;
                    expect(ReadConsoleOutputCharacterW(out, &ch, 1, {x, y}, &n) && n == 1,
                           "read border");
                    const wchar_t expected =
                        (x == 0 || x == size.X - 1 ||
                         (x == UI::DIVIDER_X && y != size.Y - 1)) ? L'+' : L'-';
                    expect(ch == expected, "horizontal border missing or overwritten");
                }
            }

            for (SHORT y = 1; y < size.Y - 1; ++y)
            {
                wchar_t ch = 0;
                DWORD n = 0;
                expect(ReadConsoleOutputCharacterW(out, &ch, 1,
                                                   {SHORT(size.X - 1), y}, &n),
                       "read right edge");
                expect(ch == ((y == 8 || y == 20 || y == size.Y - 5) ? L'+' : L'|'),
                       "right border missing");
            }

            CONSOLE_SCREEN_BUFFER_INFO info{};
            expect(GetConsoleScreenBufferInfo(out, &info), "read viewport");
            expect(info.srWindow.Top == 0 && info.srWindow.Bottom == size.Y - 1,
                   "render scrolled viewport");
            report << size.X << "x" << size.Y << ": complete borders, no scrolling\n";
        }
        report << "PASS\n";
        return 0;
    }
    catch (const exception& error)
    {
        report << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
