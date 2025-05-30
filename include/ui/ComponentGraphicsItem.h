#ifndef COMPONENTGRAPHICSITEM_H
#define COMPONENTGRAPHICSITEM_H

#include <QGraphicsItem>
#include <QObject>
#include <QVector>
#include <QPointF>

class Component;
class Node;

class ComponentGraphicsItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    enum ConnectionDirection {
        UP,
        DOWN,
        LEFT,
        RIGHT
    };

    struct ConnectionPoint {
        QPointF position;           // Local position relative to component
        int terminalIndex;          // Backend terminal index (0, 1, etc.)
        bool isOccupied;           // Whether a wire is connected
        Node* connectedNode;       // Backend node reference
        ConnectionDirection direction; // Visual direction for wire routing
    };

    explicit ComponentGraphicsItem(QGraphicsItem* parent = nullptr);
    virtual ~ComponentGraphicsItem();

    // Pure virtual interface - must be implemented by derived classes
    virtual Component* getBackendComponent() const = 0;
    virtual QString getComponentType() const = 0;

    // Connection point interface
    virtual QPointF getConnectionPointPosition(int index) const = 0;
    virtual bool isConnectionPointOccupied(int index) const = 0;
    virtual void setConnectionPointOccupied(int index, bool occupied) = 0;
    virtual int getConnectionPointAt(const QPointF& scenePos) const = 0;

    // Connection point access
    int getConnectionPointCount() const { return m_connectionPoints.size(); }
    const ConnectionPoint& getConnectionPoint(int index) const;
    ConnectionPoint& getConnectionPoint(int index);
    
    // Component identification
    QString getComponentId() const;
    QString getComponentName() const;

    // Selection and highlighting
    void setHighlighted(bool highlighted);
    bool isHighlighted() const { return m_highlighted; }

signals:
    // Interaction signals
    void connectionPointClicked(ComponentGraphicsItem* component, int pointIndex);
    void componentDoubleClicked(ComponentGraphicsItem* component);
    void componentMoved(ComponentGraphicsItem* component);
    void componentSelected(ComponentGraphicsItem* component);

protected:
    // Connection point management
    QVector<ConnectionPoint> m_connectionPoints;
    
    // Visual state
    bool m_highlighted;
    
    // Helper methods for derived classes
    void drawConnectionPoint(QPainter* painter, const ConnectionPoint& point, bool occupied = false);
    void drawSelectionIndicator(QPainter* painter, const QRectF& bounds);
    
    // Default mouse handling
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
};

#endif // COMPONENTGRAPHICSITEM_H