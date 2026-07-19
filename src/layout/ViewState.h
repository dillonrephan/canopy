#pragma once

#include <QPointF>
#include <QSizeF>
#include <QtTypes>

struct ViewState {
    QPointF pan {120.0, 35.0};
    QSizeF viewport;
    qreal zoom = 1.0;
    qreal travelDepth = 0.0;
};
