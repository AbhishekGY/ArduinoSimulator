// Implementation file: ComponentGraphicsItem.cpp
#include "ui/ComponentGraphicsItem.h"
#include "core/Component.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>

ComponentGraphicsItem::ComponentGraphicsItem(QGraphicsItem* parent)
    : QGraphicsItem(parent)
    , m_highlighted(false)
{
    // Enable standard graphics item features
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);
}

ComponentGraphicsItem::~ComponentGraphicsItem()
{
    // Cleanup is handled by Qt's graphics system
}

const ComponentGraphicsItem::ConnectionPoint& ComponentGraphicsItem::getConnectionPoint(int index) const
{
    static ConnectionPoint invalidPoint;
    if (index >= 0 && index < m_connectionPoints.size()) {
        return m_connectionPoints[index];
    }
    qWarning() << "Invalid connection point index:" << index;
    return invalidPoint;
}

ComponentGraphicsItem::ConnectionPoint& ComponentGraphicsItem::getConnectionPoint(int index)
{
    static ConnectionPoint invalidPoint;
    if (index >= 0 && index < m_connectionPoints.size()) {
        return m_connectionPoints[index];
    }
    qWarning() << "Invalid connection point index:" << index;
    return invalidPoint;
}

QString ComponentGraphicsItem::getComponentId() const
{
    Component* component = getBackendComponent();
    if (component) {
        return component->getId().toString();
    }
    return QString();
}

QString ComponentGraphicsItem::getComponentName() const
{
    Component* component = getBackendComponent();
    if (component) {
        return component->getName();
    }
    return getComponentType();
}

void ComponentGraphicsItem::setHighlighted(bool highlighted)
{
    if (m_highlighted != highlighted) {
        m_highlighted = highlighted;
        update(); // Trigger repaint
    }
}

void ComponentGraphicsItem::drawConnectionPoint(QPainter* painter, const ConnectionPoint& point, bool occupied)
{
    QPen pointPen(occupied ? Qt::darkGreen : Qt::darkGray, 1);
    QBrush pointBrush(occupied ? Qt::green : Qt::lightGray);
    
    painter->setPen(pointPen);
    painter->setBrush(pointBrush);
    
    QRectF pointRect(point.position.x() - 3, point.position.y() - 3, 6, 6);
    painter->drawEllipse(pointRect);
    
    // Draw direction indicator
    painter->setPen(QPen(Qt::black, 1));
    switch (point.direction) {
        case UP:
            painter->drawLine(point.position + QPointF(0, -6), point.position + QPointF(0, -10));
            break;
        case DOWN:
            painter->drawLine(point.position + QPointF(0, 6), point.position + QPointF(0, 10));
            break;
        case LEFT:
            painter->drawLine(point.position + QPointF(-6, 0), point.position + QPointF(-10, 0));
            break;
        case RIGHT:
            painter->drawLine(point.position + QPointF(6, 0), point.position + QPointF(10, 0));
            break;
    }
}

void ComponentGraphicsItem::drawSelectionIndicator(QPainter* painter, const QRectF& bounds)
{
    if (isSelected() || m_highlighted) {
        QPen selectionPen(m_highlighted ? Qt::red : Qt::blue, 1, Qt::DashLine);
        painter->setPen(selectionPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(bounds.adjusted(-2, -2, 2, 2));
    }
}

void ComponentGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Check for connection point clicks first
        int connectionPoint = getConnectionPointAt(event->scenePos());
        if (connectionPoint >= 0) {
            emit connectionPointClicked(this, connectionPoint);
            event->accept();
            return;
        }
        
        // Select the component
        setSelected(true);
        emit componentSelected(this);
    }
    
    QGraphicsItem::mousePressEvent(event);
}

void ComponentGraphicsItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit componentDoubleClicked(this);
        event->accept();
        return;
    }
    
    QGraphicsItem::mouseDoubleClickEvent(event);
}

QVariant ComponentGraphicsItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionChange || change == ItemPositionHasChanged) {
        emit componentMoved(this);
    }
    
    if (change == ItemSelectedChange) {
        if (value.toBool()) {
            emit componentSelected(this);
        }
    }
    
    return QGraphicsItem::itemChange(change, value);
}