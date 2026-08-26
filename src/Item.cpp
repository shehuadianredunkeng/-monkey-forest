#include "Item.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace
{
int addClampedToNonNegative(int value, int delta)
{
    const long long result = static_cast<long long>(value) + delta;
    return static_cast<int>(std::clamp(
        result,
        0LL,
        static_cast<long long>(std::numeric_limits<int>::max())));
}
} // namespace

Item::Item(std::string id, std::string name, int count)
    : Item(std::move(id), std::move(name), false, count)
{
}

Item::Item(std::string id, std::string name, bool important, int count)
    : id(std::move(id)),
      name(std::move(name)),
      important(important),
      count(std::max(0, count))
{
}

const std::string& Item::getId() const
{
    return id;
}

const std::string& Item::getName() const
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

void Item::changeCount(int delta)
{
    count = addClampedToNonNegative(count, delta);
}
