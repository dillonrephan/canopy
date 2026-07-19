#include "ThemeRegistry.h"

#include "SakuraMorningTheme.h"

const ThemeProfile &ThemeRegistry::active()
{
    return SakuraMorningTheme::profile();
}
