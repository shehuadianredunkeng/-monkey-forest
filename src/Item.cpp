#include "Item.h"

#include <algorithm>
#include <limits>
#include <utility>

using namespace std;

namespace
{
int addClampedToNonNegative(int value, int delta)
{
    const long long result = static_cast<long long>(value) + delta;
    return static_cast<int>(clamp(
        result,
        0LL,
        static_cast<long long>(numeric_limits<int>::max())));
}
} // namespace

Item::Item(string id, string name, int count)
    : Item(move(id), move(name), false, count)
{
}

Item::Item(string id, string name, bool important, int count)
    : id(move(id)),
      name(move(name)),
      important(important),
      count(max(0, count))
{
}

const string& Item::getId() const
{
    return id;
}

const string& Item::getName() const
{
    return name;
}

bool Item::isImportant() const
{
    return important;
}

int Item::getCount() const
{
    return count;
}

void Item::addCount(int delta)
{
    if (delta > 0)
    {
        count = addClampedToNonNegative(count, delta);
    }
}

void Item::reduceCount(int delta)
{
    if (delta > 0)
    {
        count = addClampedToNonNegative(count, -delta);
    }
}
