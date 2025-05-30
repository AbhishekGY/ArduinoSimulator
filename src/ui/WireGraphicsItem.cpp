#include "ui/WireGraphicsItem.h"
#include "ui/ComponentGraphicsItem.h"
#include "core/Wire.h"
#include <QPainter>
#include <QPen>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QtMath>
#include <QDebug>

WireGraphicsItem::WireGraphicsItem(QGraphicsItem* parent)
    : QGraphicsItem(parent)
    , m_backendWire(nullptr)
    , m_startComponent(nullptr)
    , m_endComponent(nullptr)
    , m_startTerminal(-1)
    , m_endTerminal(-1)
    , m_routingStyle(ORTHOGONAL)
    , m_wireWidth(2.0)
    , m_isHighlighted(false)
    , m_showCurrentFlow(false)
    , m_currentMagnitude(0.0)
    , m_currentDirection(NO_CURRENT)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);
    setZValue(-1); // Draw wires behind components
}

WireGraphicsItem::WireGraphicsItem(const QPointF& startPoint, const QPointF& endPoint, QGraphicsItem* parent)
    : WireGraphicsItem(parent)
{
    setStartPoint(startPoint);
    setEndPoint(endPoint);
    calculatePath();
}

WireGraphicsItem::~WireGraphicsItem()
{
    // Disconnect from component signals
    disconnectFromComponents();
}

QRectF WireGraphicsItem::boundingRect() const
{
    if (m_wirePath.isEmpty()) {
        return QRectF();
    }
    
    // Calculate bounding rectangle from wire path
    QRectF bounds = m_wirePath.boundingRect();
    
    // Add padding for wire width and selection indicators
    qreal padding = qMax(m_wireWidth, 5.0) + 2.0;
    return bounds.adjusted(-padding, -padding, padding, padding);
}

void WireGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    
    if (m_wirePath.isEmpty()) {
        return;
    }
    
    painter->setRenderHint(QPainter::Antialiasing);
    
    // Determine wire color and style
    QColor wireColor = Qt::black;
    Qt::PenStyle penStyle = Qt::SolidLine;
    
    if (isSelected()) {
        wireColor = Qt::blue;
    } else if (m_isHighlighted) {
        wireColor = Qt::red;
    } else if (m_backendWire && m_backendWire->getCircuit()) {
        // Color based on electrical state
        double voltage = qAbs(m_backendWire->getVoltage());
        if (voltage > 0.1) {
            wireColor = QColor(0, 100, 200); // Blue for active wires
        }
    }
    
    // Draw main wire path
    QPen wirePen(wireColor, m_wireWidth, penStyle, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(wirePen);
    painter->drawPath(m_wirePath);
    
    // Draw current flow indication
    if (m_showCurrentFlow && m_currentMagnitude > 0.001) {
        drawCurrentFlow(painter);
    }
    
    // Draw connection points
    drawConnectionPoints(painter);
    
    // Draw selection indicators
    if (isSelected()) {
        drawSelectionIndicators(painter);
    }
    
    // Draw electrical information when selected
    if (isSelected() && m_backendWire) {
        drawElectricalInfo(painter);
    }
}

void WireGraphicsItem::setStartPoint(const QPointF& point)
{
    if (m_startPoint != point) {
        prepareGeometryChange();
        m_startPoint = point;
        calculatePath();
        update();
    }
}

void WireGraphicsItem::setEndPoint(const QPointF& point)
{
    if (m_endPoint != point) {
        prepareGeometryChange();
        m_endPoint = point;
        calculatePath();
        update();
    }
}

void WireGraphicsItem::setPoints(const QPointF& startPoint, const QPointF& endPoint)
{
    if (m_startPoint != startPoint || m_endPoint != endPoint) {
        prepareGeometryChange();
        m_startPoint = startPoint;
        m_endPoint = endPoint;
        calculatePath();
        update();
    }
}

void WireGraphicsItem::setBackendWire(Wire* wire)
{
    if (m_backendWire != wire) {
        // Disconnect from old wire
        if (m_backendWire) {
            disconnect(m_backendWire, nullptr, this, nullptr);
        }
        
        m_backendWire = wire;
        
        // Connect to new wire signals
        if (m_backendWire) {
            connect(m_backendWire, &Wire::componentChanged,
                    this, &WireGraphicsItem::updateElectricalState);
            
            // Update visual state from backend
            updateElectricalState();
        }
        
        update();
    }
}

void WireGraphicsItem::connectToComponents(ComponentGraphicsItem* startComp, int startTerm,
                                         ComponentGraphicsItem* endComp, int endTerm)
{
    // Disconnect from previous components
    disconnectFromComponents();
    
    m_startComponent = startComp;
    m_startTerminal = startTerm;
    m_endComponent = endComp;  
    m_endTerminal = endTerm;
    
    if (m_startComponent && m_endComponent) {
        // Connect to component movement signals
        connect(m_startComponent, &ComponentGraphicsItem::componentMoved,
                this, &WireGraphicsItem::updateFromComponents);
        connect(m_endComponent, &ComponentGraphicsItem::componentMoved,
                this, &WireGraphicsItem::updateFromComponents);
        
        // Mark connection points as occupied
        m_startComponent->setConnectionPointOccupied(m_startTerminal, true);
        m_endComponent->setConnectionPointOccupied(m_endTerminal, true);
        
        // Update wire endpoints from components
        updateFromComponents();
        
        qDebug() << "Wire connected between" << m_startComponent->getComponentName() 
                 << "terminal" << m_startTerminal << "and" << m_endComponent->getComponentName()
                 << "terminal" << m_endTerminal;
    }
}

void WireGraphicsItem::setRoutingStyle(RoutingStyle style)
{
    if (m_routingStyle != style) {
        m_routingStyle = style;
        calculatePath();
        update();
    }
}

void WireGraphicsItem::setHighlighted(bool highlighted)
{
    if (m_isHighlighted != highlighted) {
        m_isHighlighted = highlighted;
        update();
    }
}

void WireGraphicsItem::setShowCurrentFlow(bool show)
{
    if (m_showCurrentFlow != show) {
        m_showCurrentFlow = show;
        update();
    }
}

void WireGraphicsItem::calculatePath()
{
    m_wirePath = QPainterPath();
    
    if (m_startPoint.isNull() || m_endPoint.isNull()) {
        return;
    }
    
    switch (m_routingStyle) {
        case STRAIGHT:
            calculateStraightPath();
            break;
            
        case ORTHOGONAL:
            calculateOrthogonalPath();
            break;
            
        case BEZIER:
            calculateBezierPath();
            break;
    }
}

void WireGraphicsItem::calculateStraightPath()
{
    m_wirePath.moveTo(m_startPoint);
    m_wirePath.lineTo(m_endPoint);
}

void WireGraphicsItem::calculateOrthogonalPath()
{
    // Create L-shaped or U-shaped path
    QPointF start = m_startPoint;
    QPointF end = m_endPoint;
    
    m_wirePath.moveTo(start);
    
    // Determine routing direction based on connection point directions
    qreal dx = end.x() - start.x();
    qreal dy = end.y() - start.y();
    
    // Simple L-shaped routing
    if (qAbs(dx) > qAbs(dy)) {
        // Route horizontally first, then vertically
        QPointF midPoint(end.x(), start.y());
        m_wirePath.lineTo(midPoint);
        m_wirePath.lineTo(end);
    } else {
        // Route vertically first, then horizontally  
        QPointF midPoint(start.x(), end.y());
        m_wirePath.lineTo(midPoint);
        m_wirePath.lineTo(end);
    }
}

void WireGraphicsItem::calculateBezierPath()
{
    // Create smooth curved path
    QPointF start = m_startPoint;
    QPointF end = m_endPoint;
    
    // Calculate control points for smooth curve
    qreal dx = end.x() - start.x();
    qreal dy = end.y() - start.y();
    qreal distance = qSqrt(dx * dx + dy * dy);
    
    // Control points offset based on distance
    qreal offset = qMin(distance * 0.3, 50.0);
    
    QPointF control1 = start + QPointF(offset, 0);
    QPointF control2 = end - QPointF(offset, 0);
    
    m_wirePath.moveTo(start);
    m_wirePath.cubicTo(control1, control2, end);
}

void WireGraphicsItem::drawConnectionPoints(QPainter* painter)
{
    // Draw small circles at wire endpoints
    QPen pointPen(Qt::darkGray, 1);
    QBrush pointBrush(Qt::lightGray);
    painter->setPen(pointPen);
    painter->setBrush(pointBrush);
    
    qreal radius = 3.0;
    painter->drawEllipse(m_startPoint, radius, radius);
    painter->drawEllipse(m_endPoint, radius, radius);
}

void WireGraphicsItem::drawCurrentFlow(QPainter* painter)
{
    if (m_currentDirection == NO_CURRENT || m_wirePath.isEmpty()) {
        return;
    }
    
    // Draw current flow arrows along the wire path
    QPen arrowPen(Qt::red, 1);
    painter->setPen(arrowPen);
    painter->setBrush(QBrush(Qt::red));
    
    // Calculate arrow positions along path
    qreal pathLength = m_wirePath.length();
    int numArrows = qMax(1, static_cast<int>(pathLength / 20.0)); // Arrow every 20 pixels
    
    for (int i = 0; i < numArrows; ++i) {
        qreal t = (i + 0.5) / numArrows; // Position along path (0 to 1)
        QPointF pos = m_wirePath.pointAtPercent(t);
        qreal angle = m_wirePath.angleAtPercent(t);
        
        // Adjust angle based on current direction
        if (m_currentDirection == BACKWARD) {
            angle += 180.0;
        }
        
        drawArrow(painter, pos, angle, 8.0);
    }
}

void WireGraphicsItem::drawArrow(QPainter* painter, const QPointF& pos, qreal angleDegrees, qreal size)
{
    painter->save();
    painter->translate(pos);
    painter->rotate(angleDegrees);
    
    // Draw arrow shape
    QPolygonF arrow;
    arrow << QPointF(size, 0)
          << QPointF(-size/2, size/3)
          << QPointF(-size/2, -size/3);
    
    painter->drawPolygon(arrow);
    painter->restore();
}

void WireGraphicsItem::drawSelectionIndicators(QPainter* painter)
{
    // Draw selection handles at path endpoints and midpoints
    QPen handlePen(Qt::blue, 1);
    QBrush handleBrush(Qt::cyan);
    painter->setPen(handlePen);
    painter->setBrush(handleBrush);
    
    qreal handleSize = 4.0;
    
    // Endpoint handles
    painter->drawRect(QRectF(m_startPoint - QPointF(handleSize/2, handleSize/2), 
                            QSizeF(handleSize, handleSize)));
    painter->drawRect(QRectF(m_endPoint - QPointF(handleSize/2, handleSize/2), 
                            QSizeF(handleSize, handleSize)));
    
    // Midpoint handle
    if (!m_wirePath.isEmpty()) {
        QPointF midPoint = m_wirePath.pointAtPercent(0.5);
        painter->drawEllipse(midPoint, handleSize/2, handleSize/2);
    }
}

void WireGraphicsItem::drawElectricalInfo(QPainter* painter)
{
    if (!m_backendWire) {
        return;
    }
    
    // Draw electrical information near the wire
    QPointF infoPos = m_wirePath.pointAtPercent(0.5) + QPointF(10, -10);
    
    QString info = QString("V: %1V\nI: %2mA\nR: %3Ω")
                  .arg(m_backendWire->getVoltage(), 0, 'f', 3)
                  .arg(m_backendWire->getCurrent() * 1000, 0, 'f', 2)
                  .arg(m_backendWire->getResistance(), 0, 'f', 3);
    
    // Draw info background
    QFont infoFont("Arial", 8);
    QFontMetrics fm(infoFont);
    QRectF textRect = fm.boundingRect(QRect(), Qt::AlignLeft, info);
    textRect.moveTopLeft(infoPos);
    textRect.adjust(-2, -2, 2, 2);
    
    painter->setPen(Qt::NoPen);
    painter->setBrush(QBrush(QColor(255, 255, 200, 200)));
    painter->drawRoundedRect(textRect, 3, 3);
    
    // Draw info text
    painter->setPen(QPen(Qt::black));
    painter->setFont(infoFont);
    painter->drawText(textRect, Qt::AlignLeft, info);
}

void WireGraphicsItem::updateFromComponents()
{
    if (m_startComponent && m_endComponent) {
        QPointF newStartPoint = m_startComponent->getConnectionPointPosition(m_startTerminal);
        QPointF newEndPoint = m_endComponent->getConnectionPointPosition(m_endTerminal);
        
        setPoints(newStartPoint, newEndPoint);
        
        qDebug() << "Wire updated from component movement";
    }
}

void WireGraphicsItem::updateElectricalState()
{
    if (!m_backendWire) {
        return;
    }
    
    // Update current flow visualization
    double current = m_backendWire->getCurrent();
    m_currentMagnitude = qAbs(current);
    
    if (m_currentMagnitude > 0.001) {
        m_currentDirection = (current > 0) ? FORWARD : BACKWARD;
    } else {
        m_currentDirection = NO_CURRENT;
    }
    
    // Update tooltip
    QString tooltip = QString("Wire\nVoltage: %1V\nCurrent: %2mA\nResistance: %3Ω")
                     .arg(m_backendWire->getVoltage(), 0, 'f', 3)
                     .arg(current * 1000, 0, 'f', 2)
                     .arg(m_backendWire->getResistance(), 0, 'f', 3);
    setToolTip(tooltip);
    
    update();
}

void WireGraphicsItem::disconnectFromComponents()
{
    if (m_startComponent) {
        disconnect(m_startComponent, nullptr, this, nullptr);
        m_startComponent->setConnectionPointOccupied(m_startTerminal, false);
        m_startComponent = nullptr;
    }
    
    if (m_endComponent) {
        disconnect(m_endComponent, nullptr, this, nullptr);
        m_endComponent->setConnectionPointOccupied(m_endTerminal, false);
        m_endComponent = nullptr;
    }
    
    m_startTerminal = -1;
    m_endTerminal = -1;
}

void WireGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        setSelected(true);
        event->accept();
    } else {
        QGraphicsItem::mousePressEvent(event);
    }
}

void WireGraphicsItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Open wire properties dialog
        emit wireDoubleClicked(this);
        event->accept();
    } else {
        QGraphicsItem::mouseDoubleClickEvent(event);
    }
}

void WireGraphicsItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event)
    setHighlighted(true);
}

void WireGraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event)
    setHighlighted(false);
}