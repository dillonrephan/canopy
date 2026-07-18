#include "ThemeTokens.h"

const ThemeValues &ThemeTokens::active()
{
    static const ThemeValues sakuraMorning {
        .version = 1,
        .name = QStringLiteral("Sakura Morning"),
        .sceneCanvas = QColor("#c8d3b6"),
        .shellSurface = QColor("#e5dcc5"),
        .panelSurface = QColor("#d9cfb7"),
        .raisedSurface = QColor("#efe6d2"),
        .border = QColor("#9da68b"),
        .textDark = QColor("#28352d"),
        .textLight = QColor("#fff7e8"),
        .wood = QColor("#79523f"),
        .woodHover = QColor("#936750"),
        .woodShadow = QColor("#4f372e"),
        .selection = QColor("#c85f73"),
        .hover = QColor("#f0a7b5"),
        .document = QColor("#769869"),
        .image = QColor("#e9a3b4"),
        .audio = QColor("#9877a5"),
        .code = QColor("#568877"),
        .archive = QColor("#c88b5a"),
        .flowerCenter = QColor("#e4b84e"),
        .noticeSurface = QColor("#79523f"),
        .noticeAccent = QColor("#f3c5cf"),
        .focusedBranchWidth = 13.0,
        .directoryBranchWidth = 9.0,
        .fileStemWidth = 3.0,
        .selectionHaloWidth = 6.0,
        .junctionRadius = 4.5,
        .hoveredJunctionRadius = 6.5,
        .focusAnimationDuration = 190
    };
    return sakuraMorning;
}
