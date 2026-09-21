#pragma once

#include <stdexcept>
#include <string>

using namespace std;

inline void expect(bool condition, const string& message) {
    if (!condition) {
        throw runtime_error(message);
    }
}
