#pragma once

#include <QPointF>
#include <QQuickPaintedItem>
#include <QString>
#include <QVector>
#include <QtQmlIntegration/qqmlintegration.h>

class QMouseEvent;
class QPainter;
class QWheelEvent;

class TreeSceneItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TreeScene)
    Q_PROPERTY(QString selectedName READ selectedName NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedType READ selectedType NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedPath READ selectedPath NOTIFY selectionChanged)
    Q_PROPERTY(QString breadcrumb READ breadcrumb NOTIFY selectionChanged)
    Q_PROPERTY(qreal zoom READ zoom NOTIFY cameraChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)

public:
    explicit TreeSceneItem(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    [[nodiscard]] QString selectedName() const;
    [[nodiscard]] QString selectedType() const;
    [[nodiscard]] QString selectedPath() const;
    [[nodiscard]] QString breadcrumb() const;
    [[nodiscard]] qreal zoom() const noexcept { return m_zoom; }
    [[nodiscard]] bool reducedMotion() const noexcept { return m_reducedMotion; }
    void setReducedMotion(bool reducedMotion);

    Q_INVOKABLE void resetView();

signals:
    void selectionChanged();
    void cameraChanged();
    void reducedMotionChanged();

protected:
    void hoverMoveEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    enum class Kind { Directory, Document, Image, Audio, Code, Archive };

    struct Node {
        int parent = -1;
        QString name;
        QString path;
        Kind kind = Kind::Directory;
        QPointF position;
        qreal radius = 14.0;
    };

    [[nodiscard]] QPointF toScene(const QPointF &viewPosition) const;
    [[nodiscard]] int nodeAt(const QPointF &viewPosition) const;
    [[nodiscard]] QString kindName(Kind kind) const;
    [[nodiscard]] QColor kindColor(Kind kind) const;
    void createFakeTree();
    void selectNode(int index);

    QVector<Node> m_nodes;
    QPointF m_pan {120.0, 60.0};
    QPointF m_lastPointer;
    qreal m_zoom = 1.0;
    int m_selected = 0;
    int m_hovered = -1;
    bool m_panning = false;
    bool m_reducedMotion = false;
};
