#ifndef ARDUINOGRAPHICSITEM_H
#define ARDUINOGRAPHICSITEM_H

#include "ui/ComponentGraphicsItem.h"

class Arduino;
class ArduinoPin;

class ArduinoGraphicsItem : public ComponentGraphicsItem
{
    Q_OBJECT

public:
    enum PinType {
        DIGITAL,
        ANALOG,
        POWER
    };

    struct ArduinoConnectionPoint : public ConnectionPoint {
        int pinNumber;          // Arduino pin number (0-13 for digital, 0-5 for analog)
        PinType pinType;        // Type of pin
        QString pinName;        // Name for power pins (VIN, GND, 5V, 3.3V)
    };

    explicit ArduinoGraphicsItem(Arduino* backendArduino, QGraphicsItem* parent = nullptr);

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    // ComponentGraphicsItem interface
    Component* getBackendComponent() const override;
    QString getComponentType() const override;
    QPointF getConnectionPointPosition(int index) const override;
    bool isConnectionPointOccupied(int index) const override;
    void setConnectionPointOccupied(int index, bool occupied) override;
    int getConnectionPointAt(const QPointF& scenePos) const override;

    // Arduino-specific interface
    Arduino* getBackendArduino() const { return m_backendArduino; }
    bool isPowered() const { return m_isPowered; }
    
    ArduinoPin* getBackendPin(int connectionIndex) const;
    // Pin index helpers
    int getDigitalPinIndex(int pinNumber) const;
    int getAnalogPinIndex(int pinNumber) const; 
    int getPowerPinIndex(const QString& pinName) const;
    
    // Visual options
    bool isShowingPinLabels() const { return m_showPinLabels; }
    void setShowPinLabels(bool show);
    
    bool isShowingPinStates() const { return m_showPinStates; }
    void setShowPinStates(bool show);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private slots:
    void onArduinoPowered(bool powered);
    void onPinModeChanged(int pin, int mode);
    void onPinValueChanged(int pin, double value);

private:
    void setupConnectionPoints();
    void drawPinConnectionPoint(QPainter* painter, int index);
    void drawPinLabels(QPainter* painter);
    void drawPinStates(QPainter* painter);

    // Backend reference
    Arduino* m_backendArduino;

    // Visual state
    bool m_isPowered;
    bool m_showPinLabels;
    bool m_showPinStates;
    
    // Connection points with Arduino-specific data
    QVector<ArduinoConnectionPoint> m_connectionPoints;
};

#endif // ARDUINOGRAPHICSITEM_H