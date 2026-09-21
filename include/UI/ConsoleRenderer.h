#pragma once

#include "UI/TextLayout.h"

#include <cstdint>
#include <memory>

using namespace std;

namespace UI {
using SHORT = int16_t;
constexpr SHORT UI_WIDTH = 110; // 最后一列的坐标，不是缓冲区列数
constexpr SHORT UI_HEIGHT = 34;
constexpr SHORT DIVIDER_X = 73;
constexpr SHORT INPUT_TOP = 29;
constexpr SHORT INPUT_ROW = 31;
constexpr SHORT REQUIRED_COLUMNS = UI_WIDTH + 1;

enum class Color : uint16_t {
    Normal = 7,
    Title = 11,
    Hint = 14,
    Success = 10,
    Error = 12,
    Wall = 8,
    Player = 15,
    Npc = 14,
    Item = 14,
    Quest = 12,
    Door = 11,
    Chest = 14,
    Water = 9,
    Grass = 2,
    Achievement = 13,
    Ending = 10
};
struct Rect { SHORT left; SHORT top; SHORT right; SHORT bottom; };
enum class Key { Text, VirtualText, Enter, Backspace, Delete, Left, Right, Up, Down,
                 Escape, Home, End, PageUp, PageDown, Resize, EndOfInput };
struct InputEvent { Key key; wstring text; };

class ConsoleSurface {
public:
    virtual ~ConsoleSurface() = default;
    virtual bool prepare() = 0;
    virtual Rect viewport() const { return {0, 0, UI_WIDTH, UI_HEIGHT - 1}; }
    virtual void clear(Rect area) = 0;
    virtual void write(SHORT x, SHORT y, const wstring& text, Color color) = 0;
    virtual void cursor(SHORT x, SHORT y, bool visible) = 0;
    virtual int measure(const wstring& glyph) = 0;
    virtual InputEvent readEvent() = 0;
    virtual void restore() = 0;
};

class ConsoleRenderer {
public:
    ConsoleRenderer();
    explicit ConsoleRenderer(unique_ptr<ConsoleSurface> surface);
    ~ConsoleRenderer();
    bool beginFrame(bool clearScreen = true);
    SHORT right() const { return width_; }
    SHORT height() const { return height_; }
    SHORT inputTop() const { return height_ - 5; }
    SHORT inputRow() const { return height_ - 3; }
    bool frameCleared() const { return frameCleared_; }
    void showCursor(SHORT x, SHORT y, bool visible);
    void drawText(SHORT x, SHORT y, wstring text, Color color = Color::Normal);
    void drawTextIn(Rect area, SHORT x, SHORT y, const wstring& text, Color color);
    void drawOverlayText(SHORT x, SHORT y, wstring text, Color color = Color::Normal);
    void drawHorizontalLine(SHORT y = 0, SHORT from = 0, SHORT to = -1);
    void drawVerticalLine(SHORT x = DIVIDER_X, SHORT from = 0, SHORT to = -1);
    void drawFrame();
    void clearInput();
    vector<wstring> wrapText(const wstring& text, int columns);
    int columns(const wstring& text);
    wstring clip(const wstring& text, int columns);
    InputEvent readEvent();
    void restore();
private:
    void drawOverlayTextIn(Rect area, SHORT x, SHORT y, const wstring& text,
                           Color color);

    unique_ptr<ConsoleSurface> surface_;
    bool frameReady_ = false;
    bool frameCleared_ = false;
    SHORT width_ = UI_WIDTH, height_ = UI_HEIGHT;
};
}
