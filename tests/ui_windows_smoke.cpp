// Run in an actual Windows console, not a redirected CI pipe.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "UI/GameUI.h"

#include <iostream>
#include <stdexcept>

namespace {
void expect(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
void verifyFrame() {
    const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    expect(GetConsoleScreenBufferInfo(out, &info), "cannot read real console");
    expect(info.srWindow.Left == 0 && info.srWindow.Top == 0 && info.srWindow.Right >= 110 &&
           info.srWindow.Bottom >= 29, "viewport must include all fixed coordinates");
    DWORD mode = 0;
    expect(GetConsoleMode(out, &mode) && !(mode & ENABLE_WRAP_AT_EOL_OUTPUT), "native wrapping must be disabled");
    for (SHORT y = 0; y < 30; ++y) {
        wchar_t ch = 0; DWORD read = 0;
        expect(ReadConsoleOutputCharacterW(out, &ch, 1, {110, y}, &read) && read == 1,
               "right edge unreadable");
        expect(ch == (y == 0 || y == 8 || y == 20 || y == 25 || y == 29 ? L'+' : L'|'),
               "right edge moved or was overwritten");
        if (y <= 25) {
            expect(ReadConsoleOutputCharacterW(out, &ch, 1, {73, y}, &read) && read == 1,
                   "divider unreadable");
            expect(ch == (y == 0 || y == 8 || y == 20 || y == 25 ? L'+' : L'|'),
                   "divider moved or was overwritten");
        }
    }
    expect(info.dwCursorPosition.Y < 30, "render scrolled past bottom row");
}
}

int main(int argc, char** argv) {
    try {
        const bool interactive = argc > 1 && std::string(argv[1]) == "--interactive";
        UI::ConsoleRenderer renderer;
        const std::vector<std::wstring> samples = {
            L"纯中文：清泉河谷的银色管线通往森林深处。",
            L"English: River Valley. Observe the water and listen carefully.",
            L"中英混排：输入 talk 豆豆，然后 look 观察。",
            L"数字：0123456789 80/100 60/100 2026",
            L"全角符号：【智慧】（试炼）！？：，。；「森林」",
            L"半角符号：[]()!?;:,.+-*/#",
            std::wstring(400, L'林') + L"\n长剧情结束，PageUp 查看开头。",
            L"长任务说明见右侧，面板高度不能随文本增长。",
            L"你理解了银色管线的规律。\n【智慧 +1】",
            L"【背包】\n果实 x3\n草药 x2\n藤索 x1（关键物品）\n星猿晶片 x1（关键物品）"
        };
        for (std::size_t n = 0; n < samples.size(); ++n) {
            UI::GameUI game(renderer);
            UI::GameView view;
            view.location = L"清泉河谷 River Valley";
            view.taskTitle = L"寻找豆豆 · 测试 " + std::to_wstring(n + 1) + L"/10";
            view.taskStatus = L"进行中";
            view.taskHint = n == 7 ? std::wstring(160, L'任') : L"输入 talk 豆豆";
            view.health = 80; view.stamina = 60; view.wisdom = 3; view.strength = 2;
            view.inventorySlots = 4;
            game.appendLog(samples[n]);
            if (interactive) game.appendLog(L"可截图；可输入长中文命令测试输入边界。Enter 下一项。Ctrl+C 退出。");
            expect(game.render(view), "console unavailable or too small; need 111 columns x 30 rows");
            verifyFrame();
            if (interactive && !game.readCommand()) break;
            verifyFrame();
        }
        renderer.restore();
        std::cout << "Windows console coordinate smoke checks passed.\n";
    } catch (const std::exception& error) {
        std::cerr << "Windows UI smoke test failed: " << error.what() << '\n';
        return 1;
    }
}
