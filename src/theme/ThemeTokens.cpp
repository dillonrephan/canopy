#include "ThemeTokens.h"

#include "ThemeRegistry.h"

const ThemeProfile &ThemeTokens::active()
{
    return ThemeRegistry::active();
}
