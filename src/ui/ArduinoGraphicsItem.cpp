#include "ui/ArduinoGraphicsItem.h"
#include "core/Arduino.h"
#include "core/ArduinoPin.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QFont>
#include <QPen>
#include <QBrush>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>

ArduinoGraphicsItem::ArduinoGraphicsItem(Arduino* backendArduino, QGraphicsItem* parent)
    : ComponentGraphicsItem(parent)
    , m_backendArduino(backendArduino)
    , m_isPowered(false)
    , m_showPinLabels(true)
    , m_showPinStates(false)
{
    if (!m_backendArduino) {
        qWarning() << "ArduinoGraphicsItem created with null backend Arduino";
        return;
    }
    
    // Set up the graphics item
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    
    // Initialize visual state from backend
    m_isPowered = m_backendArduino->isPoweredOn();
    
    // Set up connection points for all pins
    setupConnectionPoints();
    
    // Connect to backend Arduino signals
    connect(m_backendArduino, &Arduino::arduinoPowered,
            this, &ArduinoGraphicsItem::onArduinoPowered);
    connect(m_backendArduino, &Arduino::pinModeChanged,
            this, &ArduinoGraphicsItem::onPinModeChanged);
    connect(m_backendArduino, &Arduino::pinValueChanged,
            this, &ArduinoGraphicsItem::onPinValueChanged);
    
    qDebug() << "Created ArduinoGraphicsItem for" << m_backendArduino->getBoardName();
}

QRectF ArduinoGraphicsItem::boundingRect() const
{
    // Arduino UNO dimensions: ~160x80 pixels with pin extensions
    return QRectF(-130, -75, 260, 180);
}

void ArduinoGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    
    painter->setRenderHint(QPainter::Antialiasing);
    
    // Arduino board body
    const QRectF boardRect(-100, -45, 200, 150);
    
    // Board color based on power state
    QColor boardColor = m_isPowered ? QColor(0, 100, 0) : QColor(50, 50, 50);
    QPen boardPen(Qt::black, 2);
    QBrush boardBrush(boardColor);
    
    painter->setPen(boardPen);
    painter->setBrush(boardBrush);
    painter->drawRoundedRect(boardRect, 5, 5);
    
    // Draw board label
    painter->setPen(QPen(Qt::white, 1));
    QFont labelFont("Arial", 10, QFont::Bold);
    painter->setFont(labelFont);
    painter->drawText(boardRect, Qt::AlignCenter, "ARDUINO\nUNO");
    
    // Draw power LED indicator
    QPen ledPen(Qt::black, 1);
    painter->setPen(ledPen);
    QColor powerLedColor = m_isPowered ? Qt::green : Qt::red;
    painter->setBrush(QBrush(powerLedColor));
    painter->drawEllipse(QPointF(55, -25), 3, 3);
    
    // Draw USB connector
    painter->setPen(QPen(Qt::darkGray, 1));
    painter->setBrush(QBrush(Qt::lightGray));
    painter->drawRect(QRectF(-75, -15, 8, 20));
    
    // Draw power jack
    painter->setBrush(QBrush(Qt::black));
    painter->drawEllipse(QPointF(-65, 25), 6, 6);
    
    // Draw all pin connection points
    for (int i = 0; i < m_connectionPoints.size(); ++i) {
        drawPinConnectionPoint(painter, i);
    }
    
    // Draw pin labels if enabled
    if (m_showPinLabels) {
        drawPinLabels(painter);
    }
    
    // Draw pin states if enabled and powered
    if (m_showPinStates && m_isPowered) {
        drawPinStates(painter);
    }
    
    // Draw selection indicator
    if (isSelected()) {
        painter->setPen(QPen(Qt::blue, 1, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(boundingRect().adjusted(1, 1, -1, -1));
    }
}

ArduinoPin* ArduinoGraphicsItem::getBackendPin(int connectionIndex) const
{
    if (!m_backendArduino || connectionIndex < 0 || connectionIndex >= m_connectionPoints.size()) {
        return nullptr;
    }
    
    const ArduinoConnectionPoint& point = m_connectionPoints[connectionIndex];
    
    switch (point.pinType) {
        case PinType::DIGITAL:
            return m_backendArduino->getDigitalPin(point.pinNumber);
            
        case PinType::ANALOG:
            return m_backendArduino->getAnalogPin(point.pinNumber);
            
        case PinType::POWER:
            if (point.pinName == "GND") {
                return m_backendArduino->getGroundPin();
            } else if (point.pinName == "5V") {
                return m_backendArduino->getVccPin();
            }
            // VIN and 3.3V pins don't have backend representations in current Arduino class
            return nullptr;
            
        default:
            return nullptr;
    }
}

void ArduinoGraphicsItem::setupConnectionPoints()
{
    m_connectionPoints.clear();
    
    if (!m_backendArduino) return;
    
    // Digital pins 0-13 (right side, top to bottom)
    for (int pin = 0; pin <= 13; ++pin) {
        ArduinoConnectionPoint point;
        point.position = QPointF(110, -40 + (pin * 10)); // Right side spacing
        point.terminalIndex = pin;
        point.isOccupied = false;
        point.connectedNode = nullptr;
        point.direction = ConnectionDirection::RIGHT;
        point.pinNumber = pin;
        point.pinType = PinType::DIGITAL;
        m_connectionPoints.append(point);
    }
    
    // Analog pins A0-A5 (left side, bottom to top)
    for (int pin = 0; pin <= 5; ++pin) {
        ArduinoConnectionPoint point;
        point.position = QPointF(-110, 25 - (pin * 10)); // Left side spacing
        point.terminalIndex = 14 + pin; // Analog pins start after digital
        point.isOccupied = false;
        point.connectedNode = nullptr;
        point.direction = ConnectionDirection::LEFT;
        point.pinNumber = pin;
        point.pinType = PinType::ANALOG;
        m_connectionPoints.append(point);
    }
    
    // Power pins (top side)
    // VIN
    ArduinoConnectionPoint vinPoint;
    vinPoint.position = QPointF(-60, -55);
    vinPoint.terminalIndex = 20;
    vinPoint.isOccupied = false;
    vinPoint.connectedNode = nullptr;
    vinPoint.direction = ConnectionDirection::UP;
    vinPoint.pinNumber = -1;
    vinPoint.pinType = PinType::POWER;
    vinPoint.pinName = "VIN";
    m_connectionPoints.append(vinPoint);
    
    // GND
    ArduinoConnectionPoint gndPoint;
    gndPoint.position = QPointF(-20, -55);
    gndPoint.terminalIndex = 21;
    gndPoint.isOccupied = false;
    gndPoint.connectedNode = nullptr;
    gndPoint.direction = ConnectionDirection::UP;
    gndPoint.pinNumber = -1;
    gndPoint.pinType = PinType::POWER;
    gndPoint.pinName = "GND";
    m_connectionPoints.append(gndPoint);
    
    // 5V
    ArduinoConnectionPoint vcc5Point;
    vcc5Point.position = QPointF(20, -55);
    vcc5Point.terminalIndex = 22;
    vcc5Point.isOccupied = false;
    vcc5Point.connectedNode = nullptr;
    vcc5Point.direction = ConnectionDirection::UP;
    vcc5Point.pinNumber = -1;
    vcc5Point.pinType = PinType::POWER;
    vcc5Point.pinName = "5V";
    m_connectionPoints.append(vcc5Point);
    
    // 3.3V
    ArduinoConnectionPoint vcc33Point;
    vcc33Point.position = QPointF(60, -55);
    vcc33Point.terminalIndex = 23;
    vcc33Point.isOccupied = false;
    vcc33Point.connectedNode = nullptr;
    vcc33Point.direction = ConnectionDirection::UP;
    vcc33Point.pinNumber = -1;
    vcc33Point.pinType = PinType::POWER;
    vcc33Point.pinName = "3.3V";
    m_connectionPoints.append(vcc33Point);
    
    qDebug() << "Set up" << m_connectionPoints.size() << "connection points for Arduino";
}

void ArduinoGraphicsItem::drawPinConnectionPoint(QPainter* painter, int index)
{
    if (index < 0 || index >= m_connectionPoints.size()) return;
    
    const ArduinoConnectionPoint& point = m_connectionPoints[index];
    
    // Pin color based on type and state
    QColor pinColor = Qt::lightGray;
    if (point.isOccupied) {
        pinColor = Qt::green;
    } else {
        switch (point.pinType) {
            case PinType::DIGITAL:
                pinColor = QColor(100, 150, 255); // Light blue
                break;
            case PinType::ANALOG:
                pinColor = QColor(255, 150, 100); // Light orange
                break;
            case PinType::POWER:
                pinColor = QColor(255, 100, 100); // Light red
                break;
        }
    }
    
    QPen pinPen(Qt::black, 1);
    QBrush pinBrush(pinColor);
    painter->setPen(pinPen);
    painter->setBrush(pinBrush);
    
    // Draw connection point
    painter->drawEllipse(point.position, 4, 4);
    
    // Draw pin extension line
    painter->setPen(QPen(Qt::darkGray, 1));
    QPointF lineStart = point.position;
    QPointF lineEnd;
    
    switch (point.direction) {
        case ConnectionDirection::RIGHT:
            lineEnd = lineStart + QPointF(-10, 0);
            break;
        case ConnectionDirection::LEFT:
            lineEnd = lineStart + QPointF(10, 0);
            break;
        case ConnectionDirection::UP:
            lineEnd = lineStart + QPointF(0, 10);
            break;
        case ConnectionDirection::DOWN:
            lineEnd = lineStart + QPointF(0, -10);
            break;
    }
    
    painter->drawLine(lineStart, lineEnd);
}

void ArduinoGraphicsItem::drawPinLabels(QPainter* painter)
{
    painter->setPen(QPen(Qt::black, 1));
    QFont labelFont("Arial", 6);
    painter->setFont(labelFont);
    
    for (const ArduinoConnectionPoint& point : m_connectionPoints) {
        QString label;
        QPointF labelPos = point.position;
        
        // Determine label text and position
        if (point.pinType == PinType::DIGITAL) {
            label = QString::number(point.pinNumber);
            // Position label inside the board
            labelPos += QPointF(-15, 0);
        } else if (point.pinType == PinType::ANALOG) {
            label = QString("A%1").arg(point.pinNumber);
            // Position label inside the board
            labelPos += QPointF(15, 0);
        } else if (point.pinType == PinType::POWER) {
            label = point.pinName;
            // Position label below the pin
            labelPos += QPointF(0, 15);
        }
        
        // Draw label with background for readability
        QRectF labelRect = QFontMetrics(labelFont).boundingRect(label);
        labelRect.moveCenter(labelPos);
        labelRect.adjust(-1, -1, 1, 1);
        
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(QColor(255, 255, 255, 180)));
        painter->drawRect(labelRect);
        
        painter->setPen(QPen(Qt::black, 1));
        painter->drawText(labelRect, Qt::AlignCenter, label);
    }
}

void ArduinoGraphicsItem::drawPinStates(QPainter* painter)
{
    if (!m_backendArduino) return;
    
    painter->setPen(QPen(Qt::white, 1));
    QFont stateFont("Arial", 5);
    painter->setFont(stateFont);
    
    // Draw digital pin states
    for (int pin = 0; pin <= 13; ++pin) {
        DigitalPin* digitalPin = m_backendArduino->getDigitalPin(pin);
        if (!digitalPin) continue;
        
        QPointF statePos = m_connectionPoints[pin].position + QPointF(-25, 5);
        
        QString stateText;
        QColor stateColor = Qt::white;
        
        if (digitalPin->getMode() == ArduinoPin::OUTPUT) {
            bool pinState = digitalPin->getDigitalState();
            stateText = pinState ? "H" : "L";
            stateColor = pinState ? Qt::yellow : Qt::gray;
        } else {
            stateText = "I";
            stateColor = Qt::cyan;
        }
        
        painter->setPen(QPen(stateColor, 1));
        painter->drawText(statePos, stateText);
    }
}

Component* ArduinoGraphicsItem::getBackendComponent() const
{
    // Arduino itself is not a single component, but we return it for interface compliance
    return nullptr; // Arduino manages multiple components (pins)
}

QString ArduinoGraphicsItem::getComponentType() const
{
    return "Arduino";
}

QPointF ArduinoGraphicsItem::getConnectionPointPosition(int index) const
{
    if (index >= 0 && index < m_connectionPoints.size()) {
        return mapToScene(m_connectionPoints[index].position);
    }
    return QPointF();
}

bool ArduinoGraphicsItem::isConnectionPointOccupied(int index) const
{
    if (index >= 0 && index < m_connectionPoints.size()) {
        return m_connectionPoints[index].isOccupied;
    }
    return false;
}

void ArduinoGraphicsItem::setConnectionPointOccupied(int index, bool occupied)
{
    if (index >= 0 && index < m_connectionPoints.size()) {
        m_connectionPoints[index].isOccupied = occupied;
        update();
    }
}

int ArduinoGraphicsItem::getConnectionPointAt(const QPointF& scenePos) const
{
    const qreal connectionRadius = 8.0;
    
    for (int i = 0; i < m_connectionPoints.size(); ++i) {
        QPointF pointPos = mapToScene(m_connectionPoints[i].position);
        qreal distance = QLineF(pointPos, scenePos).length();
        
        if (distance <= connectionRadius) {
            return i;
        }
    }
    
    return -1;
}

int ArduinoGraphicsItem::getDigitalPinIndex(int pinNumber) const
{
    if (pinNumber >= 0 && pinNumber <= 13) {
        return pinNumber; // Digital pins are at indices 0-13
    }
    return -1;
}

int ArduinoGraphicsItem::getAnalogPinIndex(int pinNumber) const
{
    if (pinNumber >= 0 && pinNumber <= 5) {
        return 14 + pinNumber; // Analog pins start at index 14
    }
    return -1;
}

int ArduinoGraphicsItem::getPowerPinIndex(const QString& pinName) const
{
    if (pinName == "VIN") return 20;
    if (pinName == "GND") return 21;
    if (pinName == "5V") return 22;
    if (pinName == "3.3V") return 23;
    return -1;
}

void ArduinoGraphicsItem::setShowPinLabels(bool show)
{
    if (m_showPinLabels != show) {
        m_showPinLabels = show;
        update();
    }
}

void ArduinoGraphicsItem::setShowPinStates(bool show)
{
    if (m_showPinStates != show) {
        m_showPinStates = show;
        update();
    }
}

void ArduinoGraphicsItem::onArduinoPowered(bool powered)
{
    qDebug() << "Arduino power state changed:" << powered;
    m_isPowered = powered;
    update();
}

void ArduinoGraphicsItem::onPinModeChanged(int pin, int mode)
{
    qDebug() << "Arduino pin" << pin << "mode changed to" << mode;
    update(); // Redraw to show mode changes in pin states
}

void ArduinoGraphicsItem::onPinValueChanged(int pin, double value)
{
    Q_UNUSED(pin)
    Q_UNUSED(value)
    
    if (m_showPinStates) {
        update(); // Redraw to show value changes
    }
}

void ArduinoGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Check if clicking on a connection point
        int connectionPoint = getConnectionPointAt(event->scenePos());
        if (connectionPoint >= 0) {
            qDebug() << "Clicked on Arduino connection point" << connectionPoint;
            emit connectionPointClicked(this, connectionPoint);
            event->accept();
            return;
        }
    }
    
    ComponentGraphicsItem::mousePressEvent(event);
}

void ArduinoGraphicsItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        qDebug() << "Double-clicked Arduino - opening properties";
        emit componentDoubleClicked(this);
        event->accept();
        return;
    }
    
    ComponentGraphicsItem::mouseDoubleClickEvent(event);
}

QVariant ArduinoGraphicsItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionChange) {
        emit componentMoved(this);
    }
    
    return ComponentGraphicsItem::itemChange(change, value);
}