#include "TreeSceneItem.h"

#include <QColor>
#include <QCursor>
#include <QHoverEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <QtMath>

namespace {
constexpr qreal kMinZoom = 0.48;
constexpr qreal kMaxZoom = 2.4;
}

TreeSceneItem::TreeSceneItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton);
    setAcceptHoverEvents(true);
    setAntialiasing(true);
    createFakeTree();
}

void TreeSceneItem::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->fillRect(boundingRect(), QColor("#101713"));
    painter->save();
    painter->translate(m_pan);
    painter->scale(m_zoom, m_zoom);

    for (qsizetype i = 1; i < m_nodes.size(); ++i) {
        const Node &node = m_nodes.at(i);
        const Node &parent = m_nodes.at(node.parent);
        const QPointF delta = node.position - parent.position;
        QPainterPath branch(parent.position);
        branch.cubicTo(parent.position + QPointF(delta.x() * 0.35, -18.0),
                       node.position - QPointF(delta.x() * 0.24, 28.0),
                       node.position);
        const qreal width = node.kind == Kind::Directory ? 8.0 : 3.5;
        painter->setPen(QPen(QColor("#765a3d"), width, Qt::SolidLine, Qt::RoundCap));
        painter->drawPath(branch);
    }

    for (qsizetype i = 0; i < m_nodes.size(); ++i) {
        const Node &node = m_nodes.at(i);
        const bool selected = i == m_selected;
        const bool hovered = i == m_hovered;
        const qreal radius = node.radius + (hovered ? 3.0 : 0.0);

        painter->setPen(QPen(selected ? QColor("#f2d58f") : QColor("#1f2b23"),
                             selected ? 4.0 : 2.0));
        painter->setBrush(kindColor(node.kind));

        if (node.kind == Kind::Directory) {
            painter->drawEllipse(node.position, radius * 1.25, radius);
            painter->drawEllipse(node.position + QPointF(-radius * 0.65, 3.0),
                                 radius * 0.72,
                                 radius * 0.58);
            painter->drawEllipse(node.position + QPointF(radius * 0.68, 2.0),
                                 radius * 0.72,
                                 radius * 0.58);
        } else if (node.kind == Kind::Image) {
            for (int petal = 0; petal < 5; ++petal) {
                const qreal angle = qDegreesToRadians(petal * 72.0 - 90.0);
                const QPointF center = node.position
                    + QPointF(qCos(angle) * radius * 0.7, qSin(angle) * radius * 0.7);
                painter->drawEllipse(center, radius * 0.55, radius * 0.38);
            }
            painter->setBrush(QColor("#f2d58f"));
            painter->drawEllipse(node.position, radius * 0.38, radius * 0.38);
        } else if (node.kind == Kind::Archive) {
            QPainterPath pod;
            pod.moveTo(node.position + QPointF(0.0, -radius));
            pod.cubicTo(node.position + QPointF(radius, -radius * 0.4),
                        node.position + QPointF(radius, radius * 0.7),
                        node.position + QPointF(0.0, radius));
            pod.cubicTo(node.position + QPointF(-radius, radius * 0.7),
                        node.position + QPointF(-radius, -radius * 0.4),
                        node.position + QPointF(0.0, -radius));
            painter->drawPath(pod);
        } else {
            painter->drawEllipse(node.position, radius * 0.72, radius);
        }

        if (m_zoom >= 0.72 || selected || hovered) {
            painter->setPen(QColor("#e6eadf"));
            QFont labelFont = painter->font();
            labelFont.setPixelSize(selected ? 14 : 12);
            labelFont.setWeight(selected ? QFont::DemiBold : QFont::Normal);
            painter->setFont(labelFont);
            painter->drawText(QRectF(node.position.x() - 70.0,
                                     node.position.y() + radius + 8.0,
                                     140.0,
                                     24.0),
                              Qt::AlignHCenter | Qt::AlignTop,
                              node.name);
        }
    }

    painter->restore();
}

QString TreeSceneItem::selectedName() const
{
    return m_nodes.value(m_selected).name;
}

QString TreeSceneItem::selectedType() const
{
    return kindName(m_nodes.value(m_selected).kind);
}

QString TreeSceneItem::selectedPath() const
{
    return m_nodes.value(m_selected).path;
}

QString TreeSceneItem::breadcrumb() const
{
    return selectedPath();
}

void TreeSceneItem::setReducedMotion(bool reducedMotion)
{
    if (m_reducedMotion == reducedMotion)
        return;
    m_reducedMotion = reducedMotion;
    emit reducedMotionChanged();
}

void TreeSceneItem::resetView()
{
    m_pan = QPointF(120.0, 60.0);
    m_zoom = 1.0;
    update();
    emit cameraChanged();
}

void TreeSceneItem::hoverMoveEvent(QHoverEvent *event)
{
    const int hovered = nodeAt(event->position());
    if (hovered == m_hovered)
        return;
    m_hovered = hovered;
    update();
}

void TreeSceneItem::hoverLeaveEvent(QHoverEvent *)
{
    m_hovered = -1;
    update();
}

void TreeSceneItem::mousePressEvent(QMouseEvent *event)
{
    m_lastPointer = event->position();
    const int hit = nodeAt(event->position());
    if (event->button() == Qt::MiddleButton || hit < 0) {
        m_panning = true;
        setCursor(QCursor(Qt::ClosedHandCursor));
    } else {
        selectNode(hit);
    }
    event->accept();
}

void TreeSceneItem::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_panning)
        return;
    m_pan += event->position() - m_lastPointer;
    m_lastPointer = event->position();
    update();
    emit cameraChanged();
}

void TreeSceneItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_panning) {
        m_panning = false;
        unsetCursor();
    }
    event->accept();
}

void TreeSceneItem::wheelEvent(QWheelEvent *event)
{
    const QPointF anchor = toScene(event->position());
    const qreal factor = qPow(1.0015, event->angleDelta().y());
    m_zoom = qBound(kMinZoom, m_zoom * factor, kMaxZoom);
    m_pan = event->position() - anchor * m_zoom;
    update();
    emit cameraChanged();
    event->accept();
}

QPointF TreeSceneItem::toScene(const QPointF &viewPosition) const
{
    return (viewPosition - m_pan) / m_zoom;
}

int TreeSceneItem::nodeAt(const QPointF &viewPosition) const
{
    const QPointF scenePosition = toScene(viewPosition);
    for (qsizetype i = m_nodes.size() - 1; i >= 0; --i) {
        const Node &node = m_nodes.at(i);
        if (QLineF(scenePosition, node.position).length() <= node.radius + 8.0)
            return static_cast<int>(i);
    }
    return -1;
}

QString TreeSceneItem::kindName(Kind kind) const
{
    switch (kind) {
    case Kind::Directory: return QStringLiteral("Directory branch");
    case Kind::Document: return QStringLiteral("Document leaf");
    case Kind::Image: return QStringLiteral("Image flower");
    case Kind::Audio: return QStringLiteral("Audio berry");
    case Kind::Code: return QStringLiteral("Code fern");
    case Kind::Archive: return QStringLiteral("Archive seed pod");
    }
    return QStringLiteral("Unknown growth");
}

QColor TreeSceneItem::kindColor(Kind kind) const
{
    switch (kind) {
    case Kind::Directory: return QColor("#477a55");
    case Kind::Document: return QColor("#8aaa72");
    case Kind::Image: return QColor("#d88e9b");
    case Kind::Audio: return QColor("#9272aa");
    case Kind::Code: return QColor("#5e9c75");
    case Kind::Archive: return QColor("#b88955");
    }
    return QColor("#879083");
}

void TreeSceneItem::createFakeTree()
{
    m_nodes = {
        {-1, QStringLiteral("Home"), QStringLiteral("/home/dillon"), Kind::Directory, {420, 500}, 25},
        {0, QStringLiteral("Documents"), QStringLiteral("/home/dillon/Documents"), Kind::Directory, {190, 350}, 22},
        {0, QStringLiteral("Pictures"), QStringLiteral("/home/dillon/Pictures"), Kind::Directory, {360, 290}, 22},
        {0, QStringLiteral("Music"), QStringLiteral("/home/dillon/Music"), Kind::Directory, {525, 280}, 22},
        {0, QStringLiteral("Projects"), QStringLiteral("/home/dillon/Projects"), Kind::Directory, {700, 340}, 24},
        {1, QStringLiteral("field-notes.md"), QStringLiteral("/home/dillon/Documents/field-notes.md"), Kind::Document, {110, 205}, 13},
        {1, QStringLiteral("Receipts"), QStringLiteral("/home/dillon/Documents/Receipts"), Kind::Directory, {245, 190}, 18},
        {2, QStringLiteral("cedar-trail.jpg"), QStringLiteral("/home/dillon/Pictures/cedar-trail.jpg"), Kind::Image, {325, 130}, 14},
        {2, QStringLiteral("sketch.png"), QStringLiteral("/home/dillon/Pictures/sketch.png"), Kind::Image, {430, 145}, 14},
        {3, QStringLiteral("quiet-hours.flac"), QStringLiteral("/home/dillon/Music/quiet-hours.flac"), Kind::Audio, {535, 120}, 13},
        {4, QStringLiteral("Canopy"), QStringLiteral("/home/dillon/Projects/Canopy"), Kind::Directory, {660, 185}, 20},
        {4, QStringLiteral("archive.tar.zst"), QStringLiteral("/home/dillon/Projects/archive.tar.zst"), Kind::Archive, {790, 215}, 14},
        {10, QStringLiteral("src"), QStringLiteral("/home/dillon/Projects/Canopy/src"), Kind::Directory, {620, 65}, 17},
        {10, QStringLiteral("README.md"), QStringLiteral("/home/dillon/Projects/Canopy/README.md"), Kind::Document, {710, 75}, 12},
        {12, QStringLiteral("main.cpp"), QStringLiteral("/home/dillon/Projects/Canopy/src/main.cpp"), Kind::Code, {575, -55}, 12},
        {12, QStringLiteral("TreeSceneItem.cpp"), QStringLiteral("/home/dillon/Projects/Canopy/src/TreeSceneItem.cpp"), Kind::Code, {655, -45}, 12},
    };
}

void TreeSceneItem::selectNode(int index)
{
    if (index < 0 || index >= m_nodes.size() || index == m_selected)
        return;
    m_selected = index;
    update();
    emit selectionChanged();
}
