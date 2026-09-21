// 必须在真实 Windows 控制台里跑，重定向到管道的 CI 不算。
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "UI/GameUI.h"
#include "TestFramework.h"

#include <iostream>
#include <stdexcept>

using namespace std;

namespace {
void verifyFrame() {
    const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    expect(GetConsoleScreenBufferInfo(out, &info), "cannot read real console");
    const SHORT right = info.srWindow.Right;
    const SHORT height = info.srWindow.Bottom + 1;
    const SHORT inputTop = height - 5;
    expect(info.srWindow.Left == 0 && info.srWindow.Top == 0 &&
           info.srWindow.Right >= right && info.srWindow.Bottom >= height - 1,
           "viewport must include all fixed coordinates");
    DWORD mode = 0;
    expect(GetConsoleMode(out, &mode) && !(mode & ENABLE_WRAP_AT_EOL_OUTPUT), "native wrapping must be disabled");
    for (SHORT y = 0; y < height; ++y) {
        wchar_t ch = 0; DWORD read = 0;
        expect(ReadConsoleOutputCharacterW(out, &ch, 1, {right, y}, &read) && read == 1,
               "right edge unreadable");
        expect(ch == (y == 0 || y == 8 || y == 20 || y == inputTop || y == height - 1
                          ? L'+' : L'|'),
               "right edge moved or was overwritten");
        if (y <= inputTop) {
            expect(ReadConsoleOutputCharacterW(out, &ch, 1, {UI::DIVIDER_X, y}, &read) && read == 1,
                   "divider unreadable");
            expect(ch == (y == 0 || y == 8 || y == 20 || y == inputTop ? L'+' : L'|'),
                   "divider moved or was overwritten");
        }
    }
    expect(info.dwCursorPosition.Y < height, "render scrolled past bottom row");
}
}

int main(int argc, char** argv) {
    try {
        const bool interactive = argc > 1 && string(argv[1]) == "--interactive";
        // 先定好初始窗口尺寸；交互模式下用户可能中途调整窗口。
        const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        SMALL_RECT initialWindow{0,0,79,24};
        SetConsoleWindowInfo(out, TRUE, &initialWindow);
        SetConsoleScreenBufferSize(out, {160,80});
        SMALL_RECT baseline{0,0,110,33};
        SetConsoleWindowInfo(out, TRUE, &baseline);
        UI::ConsoleRenderer renderer;
        const vector<wstring> samples = {
            L"纯中文：清泉河谷的银色管线通往森林深处。",
            L"English: River Valley. Observe the water and listen carefully.",
            L"中英混排：输入 talk 豆豆，然后 look 观察。",
            L"数字：0123456789 80/100 60/100 2026",
            L"全角符号：【智慧】（试炼）！？：，。；「森林」",
            L"半角符号：[]()!?;:,.+-*/#",
            wstring(400, L'林') + L"\n长剧情结束，PageUp 查看开头。",
            L"长任务说明见右侧，面板高度不能随文本增长。",
            L"你理解了银色管线的规律。\n【智慧 +1】",
            L"【背包】\n果实 x3\n草药 x2\n藤索 x1（关键物品）\n星猿晶片 x1（关键物品）"
        };
        for (size_t n = 0; n < samples.size(); ++n) {
            UI::GameUI game(renderer);
            UI::GameView view;
            view.location = L"清泉河谷 River Valley";
            view.taskTitle = L"寻找豆豆 · 测试 " + to_wstring(n + 1) + L"/10";
            view.taskStatus = L"进行中";
            view.taskHint = n == 7 ? wstring(160, L'任') : L"输入 talk 豆豆";
            view.health = 80; view.stamina = 60; view.wisdom = 3; view.strength = 2;
            view.inventorySlots = 4;
            game.appendLog(samples[n]);
            if (interactive) game.appendLog(L"可截图；可输入长中文命令测试输入边界。Enter 下一项。Ctrl+C 退出。");
            expect(game.render(view), "console unavailable or too small; need 111 columns x 34 rows");
            verifyFrame();
            if (interactive && !game.readCommand()) break;
            verifyFrame();
        }
        renderer.restore();
        cout << "Windows console coordinate smoke checks passed.\n";
    } catch (const exception& error) {
        cerr << "Windows UI smoke test failed: " << error.what() << '\n';
        return 1;
    }
}

