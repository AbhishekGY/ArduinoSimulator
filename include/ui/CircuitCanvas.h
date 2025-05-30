#ifndef CIRCUITCANVAS_H
#define CIRCUITCANVAS_H

#include <QGraphicsScene>
#include <QVector>
#include <QPointF>
#include <QColor>
#include "core/Arduino.h"

class Circuit;
class ComponentGraphicsItem;
class WireGraphicsItem;
class QGraphicsSceneMouseEvent;
class QKeyEvent;

class CircuitCanvas : public QGraphicsScene
{
    Q_OBJECT

public:
    enum DrawingState {
        IDLE,           // Normal state - no drawing operation
        DRAWING_WIRE,   // Currently drawing a wire
        PLACING_COMPONENT  // Placing a new component
    };

    explicit CircuitCanvas(QObject* parent = nullptr);
    ~CircuitCanvas();

    // Circuit integration
    Circuit* getCircuit() const { return m_circuit; }
    void setCircuit(Circuit* circuit);

    // Component management
    ComponentGraphicsItem* addLED(const QPointF& position, const QColor& color = Qt::red);
    ComponentGraphicsItem* addResistor(const QPointF& position, double resistance = 1000.0);
    ComponentGraphicsItem* addArduino(const QPointF& position, Arduino::BoardType boardType = Arduino::UNO);
    void removeComponent(ComponentGraphicsItem* component);

    // Wire drawing interface
    void startWireDrawing(ComponentGraphicsItem* component, int terminal);
    void updateWireDrawing(const QPointF& mousePos);
    void completeWireDrawing(ComponentGraphicsItem* endComponent, int endTerminal);
    void cancelWireDrawing();

    // Drawing state
    DrawingState getDrawingState() const { return m_drawingState; }
    bool isDrawingWire() const { return m_drawingState == DRAWING_WIRE; }

    // Grid and snapping
    bool isGridVisible() const { return m_showGrid; }
    void setGridVisible(bool visible) { m_showGrid = visible; update(); }
    
    bool isSnapToGridEnabled() const { return m_snapToGrid; }
    void setSnapToGrid(bool enabled) { m_snapToGrid = enabled; }
    
    qreal getGridSize() const { return m_gridSize; }
    void setGridSize(qreal size) { m_gridSize = size; update(); }
    
    qreal getSnapRadius() const { return m_snapRadius; }
    void setSnapRadius(qreal radius) { m_snapRadius = radius; }

    // Component access
    const QVector<ComponentGraphicsItem*>& getComponents() const { return m_componentItems; }
    const QVector<WireGraphicsItem*>& getWires() const { return m_wireItems; }

public slots:
    void toggleGrid();

signals:
    // Wire drawing signals
    void wireDrawingStarted(ComponentGraphicsItem* component, int terminal);
    void wireDrawingCompleted(bool success);
    void wireDrawingCancelled();
    void wireCreated(WireGraphicsItem* wire);

    // Component interaction signals
    void componentPropertiesRequested(ComponentGraphicsItem* component);
    void wirePropertiesRequested(WireGraphicsItem* wire);

protected:
    // Event handling
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    // Component event handlers
    void onComponentDoubleClicked(ComponentGraphicsItem* component);
    void onComponentMoved(ComponentGraphicsItem* component);
    void onWireDoubleClicked(WireGraphicsItem* wire);
    void onCircuitChanged();

private:
    // Backend connection creation
    bool createBackendConnection(ComponentGraphicsItem* comp1, int terminal1,
                               ComponentGraphicsItem* comp2, int terminal2);

    // Helper methods
    void findSnapTarget(const QPointF& pos, ComponentGraphicsItem*& component, int& terminal);
    bool findConnectionPointAt(const QPointF& pos, ComponentGraphicsItem*& component, int& terminal);
    QPointF snapToGrid(const QPointF& pos) const;
    void clearHighlights();
    void connectComponentSignals(ComponentGraphicsItem* component);
    void removeWiresConnectedTo(ComponentGraphicsItem* component);
    void deleteSelectedItems();
    void showContextMenu(const QPointF& scenePos, const QPoint& screenPos);

    // Backend circuit
    Circuit* m_circuit;

    // Component tracking
    QVector<ComponentGraphicsItem*> m_componentItems;
    QVector<WireGraphicsItem*> m_wireItems;

    // Wire drawing state
    DrawingState m_drawingState;
    WireGraphicsItem* m_currentWire;        // Temporary wire being drawn
    ComponentGraphicsItem* m_startComponent; // Starting component for wire
    int m_startTerminal;                    // Starting terminal index

    // Grid and snapping settings
    qreal m_snapRadius;      // Radius for snapping to connection points
    qreal m_gridSize;        // Grid spacing
    bool m_showGrid;         // Whether to draw grid
    bool m_snapToGrid;       // Whether to snap to grid
    bool m_snapToComponents; // Whether to snap to connection points

    // Component ID tracking
    int m_nextComponentId;
};

#endif // CIRCUITCANVAS_H