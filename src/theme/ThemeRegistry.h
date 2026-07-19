#pragma once

#include "ThemeProfile.h"

class ThemeRegistry
{
public:
    [[nodiscard]] static const ThemeProfile &active();
};
