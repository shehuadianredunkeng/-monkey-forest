#pragma once

#include <functional>
#include <string>
#include <vector>

using namespace std;

namespace UI {
using Measure = function<int(const wstring&)>;

// 下标按代码单元算；实际宽度一律走 Measure 回调。
vector<wstring> splitGlyphs(const wstring& text);
int portableColumns(const wstring& glyph);
int displayColumns(const wstring& text, const Measure& measure);
wstring clipText(const wstring& text, int columns, const Measure& measure);
vector<wstring> wrapText(const wstring& text, int columns, const Measure& measure);

// 引擎内部一路用 UTF-8，只在 UI 这一层转成宽字符。
wstring fromUtf8(const string& text);
string toUtf8(const wstring& text);
}
