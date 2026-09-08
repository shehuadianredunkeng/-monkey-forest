#pragma once

#include <functional>
#include <string>
#include <vector>

namespace UI {
using Measure = std::function<int(const std::wstring&)>;

// Indices count code units only. Visual layout always uses the Measure callback.
std::vector<std::wstring> splitGlyphs(const std::wstring& text);
int portableColumns(const std::wstring& glyph);
int displayColumns(const std::wstring& text, const Measure& measure);
std::wstring clipText(const std::wstring& text, int columns, const Measure& measure);
std::vector<std::wstring> wrapText(const std::wstring& text, int columns, const Measure& measure);

// The existing engine speaks UTF-8; the UI boundary converts once to wide text.
std::wstring fromUtf8(const std::string& text);
std::string toUtf8(const std::wstring& text);
}
