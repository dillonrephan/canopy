#include "SakuraMorningTheme.h"

const ThemeProfile &SakuraMorningTheme::profile()
{
    static const ThemeProfile sakuraMorning {
        .version = 1,
        .id = QStringLiteral("sakura-morning"),
        .name = QStringLiteral("Sakura Morning"),
        .projection = ProjectionKind::Perspective,
        .palette = {
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
            .noticeAccent = QColor("#f3c5cf")
        },
        .branches = {
            .focusedWidth = 13.0,
            .directoryWidth = 9.0,
            .fileStemWidth = 3.0,
            .selectionHaloWidth = 6.0,
            .junctionRadius = 4.5,
            .hoveredJunctionRadius = 6.5
        },
        .labels = {
            .boundsWidth = 164.0,
            .boundsHeight = 34.0,
            .gap = 8.0,
            .primaryRevealZoom = 0.72,
            .descendantRevealZoom = 1.25,
            .rearVisibilityThreshold = 0.55,
            .selectedPixelSize = 14,
            .directoryPixelSize = 13,
            .filePixelSize = 12,
            .overflowPixelSize = 12
        },
        .motion = {
            .focusDuration = 420,
            .liveChangeDuration = 260,
            .cameraDuration = 280,
            .focusEasing = QEasingCurve::OutCubic,
            .liveChangeEasing = QEasingCurve::OutCubic,
            .cameraEasing = QEasingCurve::OutCubic
        },
        .camera = {
            .minimumZoom = 0.48,
            .maximumZoom = 5.0,
            .blankDoubleClickFactor = 1.7,
            .fogClearingZoom = 3.2,
            .wheelBase = 1.0015
        },
        .planar = {
            .focusPosition = {420.0, 500.0},
            .rootBranchStart = {420.0, 650.0},
            .parentPosition = {420.0, 635.0},
            .parentBranchStart = {420.0, 710.0},
            .parentRadius = 17.0,
            .corridorCount = 16,
            .primaryCorridorCount = 8,
            .initialLateralDistance = 150.0,
            .lateralDistanceStep = 65.0,
            .frontEndpointY = 300.0,
            .rearEndpointY = 130.0,
            .endpointYStep = 42.0,
            .firstAttachmentOffset = 14.0,
            .attachmentStep = 15.0,
            .rightAttachmentOffset = 6.0,
            .rearInitialOpacity = 0.68,
            .rearOpacityStep = 0.12
        },
        .perspective = {
            .fallbackViewportWidth = 840.0,
            .fallbackViewportHeight = 720.0,
            .centerXRatio = 0.5,
            .horizonYRatio = 0.24,
            .nearYRatio = 0.82,
            .focalDepth = 5.0,
            .cameraClearance = 1.0,
            .minimumCameraDepth = 0.05,
            .laneSpacing = 160.0,
            .descendantSpacing = 90.0,
            .fogStartDepth = 4.0,
            .fogEndDepth = 9.0,
            .minimumFogOpacity = 0.25,
            .minimumHitOpacity = 0.25,
            .fogHintOpacity = 0.48,
            .focusedTrunkNearWidth = 130.0,
            .focusedTrunkFarWidth = 82.0,
            .directoryCorridorWidth = 90.0,
            .fileCorridorWidth = 30.0,
            .corridorTipRatio = 0.48,
            .hitPadding = 10.0,
            .foregroundCorridorCount = 6,
            .fogHintCount = 2
        },
        .presentation = {
            .foregroundCorridorCount = 8,
            .fogHintCount = 8
        }
    };
    return sakuraMorning;
}
