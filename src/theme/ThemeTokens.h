#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

struct ThemeValues {
    int version = 1;
    QString name;
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
    qreal focusedBranchWidth = 13.0;
    qreal directoryBranchWidth = 9.0;
    qreal fileStemWidth = 3.0;
    qreal selectionHaloWidth = 6.0;
    qreal junctionRadius = 4.5;
    qreal hoveredJunctionRadius = 6.5;
    int focusAnimationDuration = 420;
};

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

    [[nodiscard]] static const ThemeValues &active();

    [[nodiscard]] int version() const { return active().version; }
    [[nodiscard]] QString name() const { return active().name; }
    [[nodiscard]] QColor sceneCanvas() const { return active().sceneCanvas; }
    [[nodiscard]] QColor shellSurface() const { return active().shellSurface; }
    [[nodiscard]] QColor panelSurface() const { return active().panelSurface; }
    [[nodiscard]] QColor raisedSurface() const { return active().raisedSurface; }
    [[nodiscard]] QColor border() const { return active().border; }
    [[nodiscard]] QColor textDark() const { return active().textDark; }
    [[nodiscard]] QColor textLight() const { return active().textLight; }
    [[nodiscard]] QColor wood() const { return active().wood; }
    [[nodiscard]] QColor woodHover() const { return active().woodHover; }
    [[nodiscard]] QColor woodShadow() const { return active().woodShadow; }
    [[nodiscard]] QColor selection() const { return active().selection; }
    [[nodiscard]] QColor hover() const { return active().hover; }
    [[nodiscard]] QColor document() const { return active().document; }
    [[nodiscard]] QColor image() const { return active().image; }
    [[nodiscard]] QColor audio() const { return active().audio; }
    [[nodiscard]] QColor code() const { return active().code; }
    [[nodiscard]] QColor archive() const { return active().archive; }
    [[nodiscard]] QColor flowerCenter() const { return active().flowerCenter; }
    [[nodiscard]] QColor noticeSurface() const { return active().noticeSurface; }
    [[nodiscard]] QColor noticeAccent() const { return active().noticeAccent; }
    [[nodiscard]] qreal focusedBranchWidth() const { return active().focusedBranchWidth; }
    [[nodiscard]] qreal directoryBranchWidth() const { return active().directoryBranchWidth; }
    [[nodiscard]] qreal fileStemWidth() const { return active().fileStemWidth; }
    [[nodiscard]] qreal selectionHaloWidth() const { return active().selectionHaloWidth; }
    [[nodiscard]] qreal junctionRadius() const { return active().junctionRadius; }
    [[nodiscard]] qreal hoveredJunctionRadius() const
    {
        return active().hoveredJunctionRadius;
    }
    [[nodiscard]] int focusAnimationDuration() const
    {
        return active().focusAnimationDuration;
    }
};
