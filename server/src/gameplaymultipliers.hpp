#pragma once
#include <algorithm>
#include <cmath>
#include <limits>

struct GameplayMultipliers
{
    double experience = 1.0;
    double gold = 1.0;
    double drop = 1.0;

    static int rounded(double value, int minimum)
    {
        return static_cast<int>(std::round(std::clamp(value, static_cast<double>(minimum), static_cast<double>(std::numeric_limits<int>::max()))));
    }

    int scaleExperience(int value) const
    {
        return experience == 0.0 ? 0 : rounded(value * experience, 0);
    }
};
