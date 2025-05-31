#include "ui/LEDGraphicsItem.h"
#include "core/LED.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QFont>
#include <QPen>
#include <QBrush>
#include <QGraphicsSceneMouseEvent>
#include <QtMath>
#include <QDebug>

LEDGraphicsItem::LEDGraphicsItem(LED* backendLED, QGraphicsItem* parent)
    : ComponentGraphicsItem(parent)
    , m_backendLED(backendLED)
    , m_isOn(false)
    , m_brightness(0.0)
    , m_currentColor(Qt::gray)
{
    if (!m_backendLED) {
        qWarning() << "LEDGraphicsItem created with null backend LED";
        return;
    }
    
    // Set up the graphics item
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    
    // Initialize visual state from backend
    m_isOn = m_backendLED->isOn();
    m_brightness = m_backendLED->getBrightness();
    m_currentColor = m_backendLED->getColor();
    
    // Set up connection points for LED terminals
    setupConnectionPoints();
    
    // Connect to backend LED signals for real-time updates
    connect(m_backendLED, &LED::ledStateChanged,
            this, &LEDGraphicsItem::onLEDStateChanged);
    connect(m_backendLED, &LED::componentChanged,
            this, &LEDGraphicsItem::updateVisualState);
    connect(m_backendLED, &LED::overloadDetected,
            this, [this]() {
                // Flash red to indicate overload
                m_overloadIndicator = true;
                update();
            });
    
    // Set initial size and position
    setPos(0, 0);
    
    qDebug() << "Created LEDGraphicsItem for" << m_backendLED->getName();
}

QRectF LEDGraphicsItem::boundingRect() const
{
    // LED body is 40x60 pixels (width x height)
    // Extra space for connection points and labels
    return QRectF(-40, -40, 80, 85);
}

void LEDGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    
    painter->setRenderHint(QPainter::Antialiasing);
    
    // LED body dimensions
    const qreal ledRadius = 15.0;
    const QRectF ledBody(-ledRadius, -ledRadius, ledRadius * 2, ledRadius * 2);
    
    // Draw LED body (circle)
    QPen bodyPen(Qt::black, 2);
    painter->setPen(bodyPen);
    
    // LED color based on state
    QColor ledColor;
    if (m_isOn && m_brightness > 0.01) {
        // LED is on - use actual color with brightness
        ledColor = m_currentColor;
        int alpha = static_cast<int>(255 * m_brightness);
        ledColor.setAlpha(alpha);
        
        // Add glow effect for bright LEDs
        if (m_brightness > 0.5) {
            QPen glowPen(ledColor, 4);
            glowPen.setStyle(Qt::SolidLine);
            painter->setPen(glowPen);
            painter->drawEllipse(ledBody.adjusted(-2, -2, 2, 2));
            painter->setPen(bodyPen);
        }
    } else {
        // LED is off - dark gray
        ledColor = QColor(80, 80, 80);
    }
    
    QBrush ledBrush(ledColor);
    painter->setBrush(ledBrush);
    painter->drawEllipse(ledBody);
    
    // Draw anode (+) symbol at top
    painter->setPen(QPen(Qt::white, 1.5));
    QFont symbolFont("Arial", 8, QFont::Bold);
    painter->setFont(symbolFont);
    painter->drawText(QRectF(-5, -25, 10, 10), Qt::AlignCenter, "+");
    
    // Draw cathode (-) symbol at bottom  
    painter->drawText(QRectF(-5, 15, 10, 10), Qt::AlignCenter, "-");
    
    // Draw terminal connection points
    painter->setPen(QPen(Qt::darkGray, 1));
    painter->setBrush(QBrush(Qt::lightGray));
    
    // Anode terminal (top)
    QRectF anodeTerminal(-3, -30, 6, 6);
    painter->drawEllipse(anodeTerminal);
    
    // Cathode terminal (bottom)
    QRectF cathodeTerminal(-3, 24, 6, 6);
    painter->drawEllipse(cathodeTerminal);
    
    // Draw component label
    painter->setPen(QPen(Qt::black, 1));
    QFont labelFont("Arial", 7);
    painter->setFont(labelFont);
    QString label = m_backendLED->getName();
    if (label.isEmpty()) {
        label = "LED";
    }
    painter->drawText(QRectF(-35, 30, 70, 15), Qt::AlignCenter, label);
    
    // Draw selection indicator
    if (isSelected()) {
        painter->setPen(QPen(Qt::blue, 1, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(boundingRect().adjusted(1, 1, -1, -1));
    }
    
    // Draw overload indicator
    if (m_backendLED->isOverloaded()) {
        painter->setPen(QPen(Qt::red, 3));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(ledBody.adjusted(-5, -5, 5, 5));
        
        // Draw warning symbol
        painter->setPen(QPen(Qt::red, 2));
        painter->drawText(QRectF(15, -25, 15, 15), Qt::AlignCenter, "!");
    }
    
    // Draw electrical values for debugging/info
    if (option && (option->state & QStyle::State_Selected)) {
        painter->setPen(QPen(Qt::darkBlue, 1));
        QFont infoFont("Arial", 6);
        painter->setFont(infoFont);
        
        QString info = QString("V: %1V\nI: %2mA\nP: %3mW")
                      .arg(m_backendLED->getVoltage(), 0, 'f', 2)
                      .arg(m_backendLED->getCurrent() * 1000, 0, 'f', 1)
                      .arg(m_backendLED->getPowerDissipation() * 1000, 0, 'f', 1);
        
        painter->drawText(QRectF(-35, -35, 70, 25), Qt::AlignLeft, info);
    }
}

void LEDGraphicsItem::setupConnectionPoints()
{
    // Clear existing connection points
    m_connectionPoints.clear();
    
    // Anode connection point (terminal 0) - top of LED
    ConnectionPoint anodePoint;
    anodePoint.position = QPointF(0, -27);  // Above the LED body
    anodePoint.terminalIndex = 0;
    anodePoint.isOccupied = false;
    anodePoint.connectedNode = nullptr;
    anodePoint.direction = ConnectionDirection::UP;
    m_connectionPoints.append(anodePoint);
    
    // Cathode connection point (terminal 1) - bottom of LED  
    ConnectionPoint cathodePoint;
    cathodePoint.position = QPointF(0, 27);   // Below the LED body
    cathodePoint.terminalIndex = 1;
    cathodePoint.isOccupied = false;
    cathodePoint.connectedNode = nullptr;
    cathodePoint.direction = ConnectionDirection::DOWN;
    m_connectionPoints.append(cathodePoint);
    
    qDebug() << "Set up" << m_connectionPoints.size() << "connection points for LED";
}

Component* LEDGraphicsItem::getBackendComponent() const
{
    return m_backendLED;
}

QString LEDGraphicsItem::getComponentType() const
{
    return "LED";
}

QPointF LEDGraphicsItem::getConnectionPointPosition(int index) const
{
    if (index >= 0 && index < m_connectionPoints.size()) {
        return mapToScene(m_connectionPoints[index].position);
    }
    return QPointF();
}

bool LEDGraphicsItem::isConnectionPointOccupied(int index) const
{
    if (index >= 0 && index < m_connectionPoints.size()) {
        return m_connectionPoints[index].isOccupied;
    }
    return false;
}

void LEDGraphicsItem::setConnectionPointOccupied(int index, bool occupied)
{
    if (index >= 0 && index < m_connectionPoints.size()) {
        m_connectionPoints[index].isOccupied = occupied;
        update(); // Redraw to show connection state
    }
}

int LEDGraphicsItem::getConnectionPointAt(const QPointF& scenePos) const
{
    const qreal connectionRadius = 8.0; // Radius for connection detection
    
    for (int i = 0; i < m_connectionPoints.size(); ++i) {
        QPointF pointPos = mapToScene(m_connectionPoints[i].position);
        qreal distance = QLineF(pointPos, scenePos).length();
        
        if (distance <= connectionRadius) {
            return i;
        }
    }
    
    return -1; // No connection point found
}

void LEDGraphicsItem::onLEDStateChanged(bool isOn, double brightness)
{
    qDebug() << "LED state changed - On:" << isOn << "Brightness:" << brightness;
    
    bool needsUpdate = (m_isOn != isOn) || (qAbs(m_brightness - brightness) > 0.01);
    
    m_isOn = isOn;
    m_brightness = brightness;
    
    if (needsUpdate) {
        updateVisualState();
    }
}

void LEDGraphicsItem::updateVisualState()
{
    // Update visual properties from backend
    if (m_backendLED) {
        m_isOn = m_backendLED->isOn();
        m_brightness = m_backendLED->getBrightness();
        m_currentColor = m_backendLED->getColor();
        
        // Update tooltip with current state
        QString tooltip = QString("%1\nVoltage: %2V\nCurrent: %3mA\nBrightness: %4%\nState: %5")
                         .arg(m_backendLED->getName())
                         .arg(m_backendLED->getVoltage(), 0, 'f', 2)
                         .arg(m_backendLED->getCurrent() * 1000, 0, 'f', 1)
                         .arg(m_brightness * 100, 0, 'f', 0)
                         .arg(m_isOn ? "ON" : "OFF");
        
        if (m_backendLED->isOverloaded()) {
            tooltip += "\n⚠️ OVERLOADED!";
        }
        
        setToolTip(tooltip);
    }
    
    // Trigger repaint
    update();
}

void LEDGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Check if clicking on a connection point
        int connectionPoint = getConnectionPointAt(event->scenePos());
        if (connectionPoint >= 0) {
            qDebug() << "Clicked on LED connection point" << connectionPoint;
            // Emit signal for wire drawing initiation
            emit connectionPointClicked(this, connectionPoint);
            event->accept();
            return;
        }
    }
    
    // Default handling for component selection/movement
    ComponentGraphicsItem::mousePressEvent(event);
}

void LEDGraphicsItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Double-click to open LED properties dialog
        qDebug() << "Double-clicked LED - opening properties";
        emit componentDoubleClicked(this);
        event->accept();
        return;
    }
    
    ComponentGraphicsItem::mouseDoubleClickEvent(event);
}

QVariant LEDGraphicsItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionChange) {
        // Notify that component has moved (for wire updates)
        emit componentMoved(this);
    }
    
    return ComponentGraphicsItem::itemChange(change, value);
}