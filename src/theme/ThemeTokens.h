#pragma once

#include "ThemeProfile.h"

#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>

class ThemeTokens final : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Theme)
    QML_SINGLETON

    Q_PROPERTY(int version READ version CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QColor sceneCanvas READ sceneCanvas CONSTANT)
    Q_PROPERTY(QColor shellSurface READ shellSurface CONSTANT)
    Q_PROPERTY(QColor panelSurface READ panelSurface CONSTANT)
    Q_PROPERTY(QColor raisedSurface READ raisedSurface CONSTANT)
    Q_PROPERTY(QColor border READ border CONSTANT)
    Q_PROPERTY(QColor textDark READ textDark CONSTANT)
    Q_PROPERTY(QColor textLight READ textLight CONSTANT)
    Q_PROPERTY(QColor wood READ wood CONSTANT)
    Q_PROPERTY(QColor woodHover READ woodHover CONSTANT)
    Q_PROPERTY(QColor woodShadow READ woodShadow CONSTANT)
    Q_PROPERTY(QColor selection READ selection CONSTANT)
    Q_PROPERTY(QColor hover READ hover CONSTANT)
    Q_PROPERTY(QColor document READ document CONSTANT)
    Q_PROPERTY(QColor image READ image CONSTANT)
    Q_PROPERTY(QColor audio READ audio CONSTANT)
    Q_PROPERTY(QColor code READ code CONSTANT)
    Q_PROPERTY(QColor archive READ archive CONSTANT)
    Q_PROPERTY(QColor flowerCenter READ flowerCenter CONSTANT)
    Q_PROPERTY(QColor noticeSurface READ noticeSurface CONSTANT)
    Q_PROPERTY(QColor noticeAccent READ noticeAccent CONSTANT)
    Q_PROPERTY(qreal focusedBranchWidth READ focusedBranchWidth CONSTANT)
    Q_PROPERTY(qreal directoryBranchWidth READ directoryBranchWidth CONSTANT)
    Q_PROPERTY(qreal fileStemWidth READ fileStemWidth CONSTANT)
    Q_PROPERTY(qreal selectionHaloWidth READ selectionHaloWidth CONSTANT)
    Q_PROPERTY(qreal junctionRadius READ junctionRadius CONSTANT)
    Q_PROPERTY(qreal hoveredJunctionRadius READ hoveredJunctionRadius CONSTANT)
    Q_PROPERTY(int focusAnimationDuration READ focusAnimationDuration CONSTANT)

public:
    explicit ThemeTokens(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    [[nodiscard]] static const ThemeProfile &active();

    [[nodiscard]] int version() const { return active().version; }
    [[nodiscard]] QString name() const { return active().name; }
    [[nodiscard]] QColor sceneCanvas() const { return active().palette.sceneCanvas; }
    [[nodiscard]] QColor shellSurface() const { return active().palette.shellSurface; }
    [[nodiscard]] QColor panelSurface() const { return active().palette.panelSurface; }
    [[nodiscard]] QColor raisedSurface() const { return active().palette.raisedSurface; }
    [[nodiscard]] QColor border() const { return active().palette.border; }
    [[nodiscard]] QColor textDark() const { return active().palette.textDark; }
    [[nodiscard]] QColor textLight() const { return active().palette.textLight; }
    [[nodiscard]] QColor wood() const { return active().palette.wood; }
    [[nodiscard]] QColor woodHover() const { return active().palette.woodHover; }
    [[nodiscard]] QColor woodShadow() const { return active().palette.woodShadow; }
    [[nodiscard]] QColor selection() const { return active().palette.selection; }
    [[nodiscard]] QColor hover() const { return active().palette.hover; }
    [[nodiscard]] QColor document() const { return active().palette.document; }
    [[nodiscard]] QColor image() const { return active().palette.image; }
    [[nodiscard]] QColor audio() const { return active().palette.audio; }
    [[nodiscard]] QColor code() const { return active().palette.code; }
    [[nodiscard]] QColor archive() const { return active().palette.archive; }
    [[nodiscard]] QColor flowerCenter() const { return active().palette.flowerCenter; }
    [[nodiscard]] QColor noticeSurface() const { return active().palette.noticeSurface; }
    [[nodiscard]] QColor noticeAccent() const { return active().palette.noticeAccent; }
    [[nodiscard]] qreal focusedBranchWidth() const { return active().branches.focusedWidth; }
    [[nodiscard]] qreal directoryBranchWidth() const { return active().branches.directoryWidth; }
    [[nodiscard]] qreal fileStemWidth() const { return active().branches.fileStemWidth; }
    [[nodiscard]] qreal selectionHaloWidth() const
    {
        return active().branches.selectionHaloWidth;
    }
    [[nodiscard]] qreal junctionRadius() const
    {
        return active().branches.junctionRadius;
    }
    [[nodiscard]] qreal hoveredJunctionRadius() const
    {
        return active().branches.hoveredJunctionRadius;
    }
    [[nodiscard]] int focusAnimationDuration() const
    {
        return active().motion.focusDuration;
    }
};
