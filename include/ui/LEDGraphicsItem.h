#ifndef LEDGRAPHICSITEM_H
#define LEDGRAPHICSITEM_H

#include "ui/ComponentGraphicsItem.h"
#include <QColor>

class LED;

class LEDGraphicsItem : public ComponentGraphicsItem
{
    Q_OBJECT

public:
    explicit LEDGraphicsItem(LED* backendLED, QGraphicsItem* parent = nullptr);

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

    // LED-specific interface
    LED* getBackendLED() const { return m_backendLED; }
    bool isLEDOn() const { return m_isOn; }
    double getBrightness() const { return m_brightness; }
    QColor getCurrentColor() const { return m_currentColor; }

protected:
    // Mouse events for interaction
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private slots:
    void onLEDStateChanged(bool isOn, double brightness);
    void updateVisualState();

private:
    void setupConnectionPoints();

    // Backend reference
    LED* m_backendLED;

    // Visual state
    bool m_isOn;
    double m_brightness;
    QColor m_currentColor;
    bool m_overloadIndicator;
};

#endif // LEDGRAPHICSITEM_H