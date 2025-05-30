#ifndef WIREGRAPHICSITEM_H
#define WIREGRAPHICSITEM_H

#include <QGraphicsItem>
#include <QObject>
#include <QPainterPath>
#include <QPointF>

class Wire;
class ComponentGraphicsItem;

class WireGraphicsItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    enum RoutingStyle {
        STRAIGHT,    // Direct line between points
        ORTHOGONAL,  // L-shaped or U-shaped routing
        BEZIER       // Smooth curved path
    };

    enum CurrentDirection {
        NO_CURRENT,
        FORWARD,     // Current from start to end
        BACKWARD     // Current from end to start
    };

    explicit WireGraphicsItem(QGraphicsItem* parent = nullptr);
    explicit WireGraphicsItem(const QPointF& startPoint, const QPointF& endPoint, QGraphicsItem* parent = nullptr);
    ~WireGraphicsItem();

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    // Wire endpoints
    QPointF getStartPoint() const { return m_startPoint; }
    QPointF getEndPoint() const { return m_endPoint; }
    void setStartPoint(const QPointF& point);
    void setEndPoint(const QPointF& point);
    void setPoints(const QPointF& startPoint, const QPointF& endPoint);

    // Backend integration
    Wire* getBackendWire() const { return m_backendWire; }
    void setBackendWire(Wire* wire);

    // Component connections
    void connectToComponents(ComponentGraphicsItem* startComp, int startTerm,
                           ComponentGraphicsItem* endComp, int endTerm);
    ComponentGraphicsItem* getStartComponent() const { return m_startComponent; }
    ComponentGraphicsItem* getEndComponent() const { return m_endComponent; }
    int getStartTerminal() const { return m_startTerminal; }
    int getEndTerminal() const { return m_endTerminal; }

    // Visual properties
    RoutingStyle getRoutingStyle() const { return m_routingStyle; }
    void setRoutingStyle(RoutingStyle style);
    
    qreal getWireWidth() const { return m_wireWidth; }
    void setWireWidth(qreal width) { m_wireWidth = width; update(); }
    
    bool isHighlighted() const { return m_isHighlighted; }
    void setHighlighted(bool highlighted);
    
    bool isShowingCurrentFlow() const { return m_showCurrentFlow; }
    void setShowCurrentFlow(bool show);

    // Wire path access
    const QPainterPath& getWirePath() const { return m_wirePath; }

signals:
    void wireDoubleClicked(WireGraphicsItem* wire);
    void wireSelectionChanged(WireGraphicsItem* wire, bool selected);

protected:
    // Mouse events
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private slots:
    void updateFromComponents();
    void updateElectricalState();

private:
    // Path calculation methods
    void calculatePath();
    void calculateStraightPath();
    void calculateOrthogonalPath();
    void calculateBezierPath();

    // Drawing helper methods
    void drawConnectionPoints(QPainter* painter);
    void drawCurrentFlow(QPainter* painter);
    void drawArrow(QPainter* painter, const QPointF& pos, qreal angleDegrees, qreal size);
    void drawSelectionIndicators(QPainter* painter);
    void drawElectricalInfo(QPainter* painter);

    // Component management
    void disconnectFromComponents();

    // Backend reference
    Wire* m_backendWire;

    // Component connections
    ComponentGraphicsItem* m_startComponent;
    ComponentGraphicsItem* m_endComponent;
    int m_startTerminal;
    int m_endTerminal;

    // Wire geometry
    QPointF m_startPoint;
    QPointF m_endPoint;
    QPainterPath m_wirePath;
    RoutingStyle m_routingStyle;

    // Visual properties
    qreal m_wireWidth;
    bool m_isHighlighted;

    // Electrical visualization
    bool m_showCurrentFlow;
    double m_currentMagnitude;
    CurrentDirection m_currentDirection;
};

#endif // WIREGRAPHICSITEM_H