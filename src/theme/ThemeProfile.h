#pragma once

#include <QColor>
#include <QEasingCurve>
#include <QPointF>
#include <QString>
#include <QtTypes>

enum class ProjectionKind {
    Planar,
    Perspective
};

struct ThemePalette {
    QColor sceneCanvas;
    QColor shellSurface;
    QColor panelSurface;
    QColor raisedSurface;
    QColor border;
    QColor textDark;
    QColor textLight;
    QColor wood;
    QColor woodHover;
    QColor woodShadow;
    QColor selection;
    QColor hover;
    QColor document;
    QColor image;
    QColor audio;
    QColor code;
    QColor archive;
    QColor flowerCenter;
    QColor noticeSurface;
    QColor noticeAccent;
};

struct BranchStyle {
    qreal focusedWidth = 13.0;
    qreal directoryWidth = 9.0;
    qreal fileStemWidth = 3.0;
    qreal selectionHaloWidth = 6.0;
    qreal junctionRadius = 4.5;
    qreal hoveredJunctionRadius = 6.5;
};

struct LabelStyle {
    qreal boundsWidth = 164.0;
    qreal boundsHeight = 34.0;
    qreal gap = 8.0;
    qreal primaryRevealZoom = 0.72;
    qreal descendantRevealZoom = 1.25;
    qreal rearVisibilityThreshold = 0.55;
    int selectedPixelSize = 14;
    int directoryPixelSize = 13;
    int filePixelSize = 12;
    int overflowPixelSize = 12;
};

struct MotionProfile {
    int focusDuration = 420;
    int liveChangeDuration = 260;
    int cameraDuration = 280;
    QEasingCurve::Type focusEasing = QEasingCurve::OutCubic;
    QEasingCurve::Type liveChangeEasing = QEasingCurve::OutCubic;
    QEasingCurve::Type cameraEasing = QEasingCurve::OutCubic;
};

struct CameraProfile {
    qreal minimumZoom = 0.48;
    qreal maximumZoom = 5.0;
    qreal blankDoubleClickFactor = 1.7;
    qreal fogClearingZoom = 3.2;
    qreal wheelBase = 1.0015;
};

struct PlanarLayoutProfile {
    QPointF focusPosition {420.0, 500.0};
    QPointF rootBranchStart {420.0, 650.0};
    QPointF parentPosition {420.0, 635.0};
    QPointF parentBranchStart {420.0, 710.0};
    qreal parentRadius = 17.0;
    int corridorCount = 16;
    int primaryCorridorCount = 8;
    qreal initialLateralDistance = 150.0;
    qreal lateralDistanceStep = 65.0;
    qreal frontEndpointY = 300.0;
    qreal rearEndpointY = 130.0;
    qreal endpointYStep = 42.0;
    qreal firstAttachmentOffset = 14.0;
    qreal attachmentStep = 15.0;
    qreal rightAttachmentOffset = 6.0;
    qreal rearInitialOpacity = 0.68;
    qreal rearOpacityStep = 0.12;
};

struct PerspectiveProfile {
    qreal fallbackViewportWidth = 840.0;
    qreal fallbackViewportHeight = 720.0;
    qreal centerXRatio = 0.5;
    qreal horizonYRatio = 0.24;
    qreal nearYRatio = 0.82;
    qreal focalDepth = 5.0;
    qreal cameraClearance = 1.0;
    qreal minimumCameraDepth = 0.05;
    qreal laneSpacing = 160.0;
    qreal descendantSpacing = 90.0;
    qreal fogStartDepth = 4.0;
    qreal fogEndDepth = 9.0;
    qreal minimumFogOpacity = 0.25;
    qreal minimumHitOpacity = 0.25;
    qreal fogHintOpacity = 0.48;
    qreal focusedTrunkNearWidth = 130.0;
    qreal focusedTrunkFarWidth = 82.0;
    qreal directoryCorridorWidth = 90.0;
    qreal fileCorridorWidth = 30.0;
    qreal corridorTipRatio = 0.48;
    qreal hitPadding = 10.0;
    int foregroundCorridorCount = 6;
    int fogHintCount = 2;
};

struct PresentationProfile {
    int foregroundCorridorCount = 8;
    int fogHintCount = 8;
};

struct ThemeProfile {
    int version = 1;
    QString id;
    QString name;
    ProjectionKind projection = ProjectionKind::Planar;
    ThemePalette palette;
    BranchStyle branches;
    LabelStyle labels;
    MotionProfile motion;
    CameraProfile camera;
    PlanarLayoutProfile planar;
    PerspectiveProfile perspective;
    PresentationProfile presentation;
};
