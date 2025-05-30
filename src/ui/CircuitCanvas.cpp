#include "ui/CircuitCanvas.h"
#include "ui/ComponentGraphicsItem.h"
#include "ui/LEDGraphicsItem.h"
#include "ui/ResistorGraphicsItem.h"
#include "ui/ArduinoGraphicsItem.h"
#include "ui/WireGraphicsItem.h"
#include "simulation/Circuit.h"
#include "simulation/Node.h"
#include "core/LED.h"
#include "core/Resistor.h"
#include "core/Wire.h"
#include "core/Arduino.h"
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QtMath>

CircuitCanvas::CircuitCanvas(QObject* parent)
    : QGraphicsScene(parent)
    , m_circuit(nullptr)
    , m_drawingState(IDLE)
    , m_currentWire(nullptr)
    , m_startComponent(nullptr)
    , m_startTerminal(-1)
    , m_snapRadius(15.0)
    , m_gridSize(20.0)
    , m_showGrid(true)
    , m_snapToGrid(true)
    , m_snapToComponents(true)
    , m_nextComponentId(1)
{
    // Set scene size (can be made configurable)
    setSceneRect(-2000, -1500, 4000, 3000);
    
    // Set background
    setBackgroundBrush(QBrush(Qt::white));
    
    qDebug() << "CircuitCanvas created";
}

CircuitCanvas::~CircuitCanvas()
{
    // Cleanup is handled by Qt's scene management
    qDebug() << "CircuitCanvas destroyed";
}

void CircuitCanvas::setCircuit(Circuit* circuit)
{
    if (m_circuit != circuit) {
        // Disconnect from old circuit
        if (m_circuit) {
            disconnect(m_circuit, nullptr, this, nullptr);
        }
        
        m_circuit = circuit;
        
        // Connect to new circuit
        if (m_circuit) {
            connect(m_circuit, &Circuit::circuitChanged,
                    this, &CircuitCanvas::onCircuitChanged);
        }
        
        qDebug() << "Circuit set for canvas";
    }
}

// Component management
ComponentGraphicsItem* CircuitCanvas::addLED(const QPointF& position, const QColor& color)
{
    if (!m_circuit) {
        qWarning() << "Cannot add LED: no circuit set";
        return nullptr;
    }
    
    // Create backend LED
    LED* backendLED = LED::createStandardLED(color.name(), m_circuit);
    backendLED->setName(QString("LED%1").arg(m_nextComponentId++));
    
    // Add to circuit
    m_circuit->addComponent(backendLED);
    
    // Create graphics item
    LEDGraphicsItem* ledGraphics = new LEDGraphicsItem(backendLED);
    ledGraphics->setPos(snapToGrid(position));
    
    // Connect signals
    connectComponentSignals(ledGraphics);
    
    // Add to scene
    addItem(ledGraphics);
    m_componentItems.append(ledGraphics);
    
    qDebug() << "Added LED at position" << position;
    return ledGraphics;
}

ComponentGraphicsItem* CircuitCanvas::addResistor(const QPointF& position, double resistance)
{
    if (!m_circuit) {
        qWarning() << "Cannot add resistor: no circuit set";
        return nullptr;
    }
    
    // Create backend resistor
    Resistor* backendResistor = new Resistor(resistance, m_circuit);
    backendResistor->setName(QString("R%1").arg(m_nextComponentId++));
    
    // Add to circuit
    m_circuit->addComponent(backendResistor);
    
    // Create graphics item (assuming ResistorGraphicsItem exists)
    // ResistorGraphicsItem* resistorGraphics = new ResistorGraphicsItem(backendResistor);
    // For now, create a placeholder that returns nullptr until ResistorGraphicsItem is implemented
    qDebug() << "Resistor graphics item not yet implemented";
    return nullptr;
}

ComponentGraphicsItem* CircuitCanvas::addArduino(const QPointF& position, Arduino::BoardType boardType)
{
    if (!m_circuit) {
        qWarning() << "Cannot add Arduino: no circuit set";
        return nullptr;
    }
    
    // Create backend Arduino
    Arduino* backendArduino = new Arduino(boardType, m_circuit);
    
    // Set circuit for Arduino (this adds all pins as components)
    backendArduino->setCircuit(m_circuit);
    
    // Create graphics item (assuming ArduinoGraphicsItem exists)
    // ArduinoGraphicsItem* arduinoGraphics = new ArduinoGraphicsItem(backendArduino);
    // For now, create a placeholder
    qDebug() << "Arduino graphics item not yet implemented";
    return nullptr;
}

void CircuitCanvas::removeComponent(ComponentGraphicsItem* component)
{
    if (!component || !m_circuit) {
        return;
    }
    
    // Remove any connected wires first
    removeWiresConnectedTo(component);
    
    // Remove from backend circuit
    Component* backendComponent = component->getBackendComponent();
    if (backendComponent) {
        m_circuit->removeComponent(backendComponent);
    }
    
    // Remove from scene and tracking
    m_componentItems.removeOne(component);
    removeItem(component);
    component->deleteLater();
    
    qDebug() << "Removed component";
}

// Wire drawing implementation
void CircuitCanvas::startWireDrawing(ComponentGraphicsItem* component, int terminal)
{
    if (m_drawingState != IDLE) {
        qDebug() << "Already in drawing state, ignoring wire start";
        return;
    }
    
    if (!component || terminal < 0) {
        qWarning() << "Invalid component or terminal for wire drawing";
        return;
    }
    
    // Check if connection point is already occupied
    if (component->isConnectionPointOccupied(terminal)) {
        qDebug() << "Connection point already occupied";
        return;
    }
    
    // Start wire drawing
    m_drawingState = DRAWING_WIRE;
    m_startComponent = component;
    m_startTerminal = terminal;
    
    // Create temporary wire for visual feedback
    QPointF startPos = component->getConnectionPointPosition(terminal);
    m_currentWire = new WireGraphicsItem(startPos, startPos);
    m_currentWire->setRoutingStyle(WireGraphicsItem::ORTHOGONAL);
    addItem(m_currentWire);
    
    // Highlight the starting component
    component->setHighlighted(true);
    
    qDebug() << "Started wire drawing from" << component->getComponentName()
             << "terminal" << terminal;
    
    emit wireDrawingStarted(component, terminal);
}

void CircuitCanvas::updateWireDrawing(const QPointF& mousePos)
{
    if (m_drawingState != DRAWING_WIRE || !m_currentWire) {
        return;
    }
    
    QPointF endPos = mousePos;
    
    // Check for snap to connection points
    ComponentGraphicsItem* snapComponent = nullptr;
    int snapTerminal = -1;
    
    if (m_snapToComponents) {
        findSnapTarget(mousePos, snapComponent, snapTerminal);
        if (snapComponent && snapTerminal >= 0) {
            endPos = snapComponent->getConnectionPointPosition(snapTerminal);
            
            // Highlight snap target
            clearHighlights();
            m_startComponent->setHighlighted(true);
            snapComponent->setHighlighted(true);
        } else {
            // Clear highlights except start component
            clearHighlights();
            m_startComponent->setHighlighted(true);
        }
    }
    
    // Snap to grid if enabled
    if (m_snapToGrid && (!snapComponent || snapTerminal < 0)) {
        endPos = snapToGrid(endPos);
    }
    
    // Update temporary wire
    m_currentWire->setEndPoint(endPos);
}

void CircuitCanvas::completeWireDrawing(ComponentGraphicsItem* endComponent, int endTerminal)
{
    if (m_drawingState != DRAWING_WIRE || !m_currentWire || !m_startComponent) {
        qDebug() << "Cannot complete wire drawing - invalid state";
        return;
    }
    
    bool success = false;
    
    if (endComponent && endTerminal >= 0) {
        // Validate connection
        if (endComponent == m_startComponent && endTerminal == m_startTerminal) {
            qDebug() << "Cannot connect component to itself";
        } else if (endComponent->isConnectionPointOccupied(endTerminal)) {
            qDebug() << "End connection point already occupied";
        } else {
            // Create the electrical connection in backend
            success = createBackendConnection(m_startComponent, m_startTerminal,
                                            endComponent, endTerminal);
        }
    }
    
    if (success) {
        // Convert temporary wire to permanent wire
        WireGraphicsItem* permanentWire = m_currentWire;
        permanentWire->connectToComponents(m_startComponent, m_startTerminal,
                                         endComponent, endTerminal);
        
        // Connect wire signals
        connect(permanentWire, &WireGraphicsItem::wireDoubleClicked,
                this, &CircuitCanvas::onWireDoubleClicked);
        
        m_wireItems.append(permanentWire);
        m_currentWire = nullptr;
        
        qDebug() << "Wire drawing completed successfully";
        emit wireCreated(permanentWire);
    } else {
        // Remove temporary wire
        removeItem(m_currentWire);
        m_currentWire->deleteLater();
        m_currentWire = nullptr;
        
        qDebug() << "Wire drawing failed";
    }
    
    // Reset drawing state
    m_drawingState = IDLE;
    m_startComponent = nullptr;
    m_startTerminal = -1;
    
    clearHighlights();
    emit wireDrawingCompleted(success);
}

void CircuitCanvas::cancelWireDrawing()
{
    if (m_drawingState != DRAWING_WIRE) {
        return;
    }
    
    // Remove temporary wire
    if (m_currentWire) {
        removeItem(m_currentWire);
        m_currentWire->deleteLater();
        m_currentWire = nullptr;
    }
    
    // Reset state
    m_drawingState = IDLE;
    m_startComponent = nullptr;
    m_startTerminal = -1;
    
    clearHighlights();
    
    qDebug() << "Wire drawing cancelled";
    emit wireDrawingCancelled();
}

bool CircuitCanvas::createBackendConnection(ComponentGraphicsItem* comp1, int terminal1,
                                          ComponentGraphicsItem* comp2, int terminal2)
{
    if (!m_circuit || !comp1 || !comp2) {
        qWarning() << "Cannot create backend connection: missing circuit or components";
        return false;
    }
    
    Component* backendComp1 = comp1->getBackendComponent();
    Component* backendComp2 = comp2->getBackendComponent();
    
    if (!backendComp1 || !backendComp2) {
        qWarning() << "Cannot create backend connection: components have no backend";
        return false;
    }
    
    // Get existing nodes if components are already connected
    Node* node1 = backendComp1->getNode(terminal1);
    Node* node2 = backendComp2->getNode(terminal2);
    
    Node* connectionNode = nullptr;
    
    if (!node1 && !node2) {
        // Case 1: Neither component connected - create new node
        connectionNode = m_circuit->createNode();
        backendComp1->connectToNode(connectionNode, terminal1);
        backendComp2->connectToNode(connectionNode, terminal2);
        
        qDebug() << "Created new node" << connectionNode->getId() 
                 << "connecting" << backendComp1->getName() 
                 << "to" << backendComp2->getName();
        
    } else if (node1 && !node2) {
        // Case 2: First component already connected - use its node
        connectionNode = node1;
        backendComp2->connectToNode(connectionNode, terminal2);
        
        qDebug() << "Connected" << backendComp2->getName() 
                 << "to existing node" << connectionNode->getId();
        
    } else if (!node1 && node2) {
        // Case 3: Second component already connected - use its node
        connectionNode = node2;
        backendComp1->connectToNode(connectionNode, terminal1);
        
        qDebug() << "Connected" << backendComp1->getName() 
                 << "to existing node" << connectionNode->getId();
        
    } else if (node1 == node2) {
        // Case 4: Both connected to same node - already connected
        qDebug() << "Components already connected via node" << node1->getId();
        connectionNode = node1;
        
    } else {
        // Case 5: Both connected to different nodes - merge nodes
        connectionNode = node1;
        
        // Move all connections from node2 to node1
        QVector<QPair<Component*, int>> connections = node2->getConnections();
        for (const auto& conn : connections) {
            conn.first->disconnectFromNode(conn.second);
            conn.first->connectToNode(connectionNode, conn.second);
        }
        
        // Remove the now-empty node2
        m_circuit->removeNode(node2);
        
        qDebug() << "Merged nodes - now using node" << connectionNode->getId();
    }
    
    // Create backend wire component
    Wire* backendWire = m_circuit->addWire(connectionNode, connectionNode);
    if (backendWire && m_currentWire) {
        m_currentWire->setBackendWire(backendWire);
    }
    
    // Update connection point states
    comp1->setConnectionPointOccupied(terminal1, true);
    comp2->setConnectionPointOccupied(terminal2, true);
    
    return true;
}

// Mouse event handling
void CircuitCanvas::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_drawingState == DRAWING_WIRE) {
            // Check if clicking on a valid connection point
            ComponentGraphicsItem* component = nullptr;
            int terminal = -1;
            
            if (findConnectionPointAt(event->scenePos(), component, terminal)) {
                completeWireDrawing(component, terminal);
            } else {
                // Click in empty space - cancel wire drawing
                cancelWireDrawing();
            }
            
            event->accept();
            return;
        }
    } else if (event->button() == Qt::RightButton) {
        if (m_drawingState == DRAWING_WIRE) {
            // Right click cancels wire drawing
            cancelWireDrawing();
            event->accept();
            return;
        } else {
            // Show context menu
            showContextMenu(event->scenePos(), event->screenPos());
            event->accept();
            return;
        }
    }
    
    // Default handling
    QGraphicsScene::mousePressEvent(event);
}

void CircuitCanvas::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_drawingState == DRAWING_WIRE) {
        updateWireDrawing(event->scenePos());
        event->accept();
        return;
    }
    
    // Default handling
    QGraphicsScene::mouseMoveEvent(event);
}

void CircuitCanvas::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
        case Qt::Key_Escape:
            if (m_drawingState == DRAWING_WIRE) {
                cancelWireDrawing();
                event->accept();
                return;
            }
            break;
            
        case Qt::Key_Delete:
            deleteSelectedItems();
            event->accept();
            return;
            
        case Qt::Key_G:
            if (event->modifiers() & Qt::ControlModifier) {
                toggleGrid();
                event->accept();
                return;
            }
            break;
    }
    
    QGraphicsScene::keyPressEvent(event);
}

// Helper methods
void CircuitCanvas::findSnapTarget(const QPointF& pos, ComponentGraphicsItem*& component, int& terminal)
{
    component = nullptr;
    terminal = -1;
    
    // Find closest connection point within snap radius
    qreal minDistance = m_snapRadius;
    
    for (ComponentGraphicsItem* comp : m_componentItems) {
        if (comp == m_startComponent) continue; // Skip starting component
        
        for (int i = 0; i < comp->getConnectionPointCount(); ++i) {
            if (comp->isConnectionPointOccupied(i)) continue; // Skip occupied points
            
            QPointF pointPos = comp->getConnectionPointPosition(i);
            qreal distance = QLineF(pos, pointPos).length();
            
            if (distance < minDistance) {
                minDistance = distance;
                component = comp;
                terminal = i;
            }
        }
    }
}

bool CircuitCanvas::findConnectionPointAt(const QPointF& pos, ComponentGraphicsItem*& component, int& terminal)
{
    component = nullptr;
    terminal = -1;
    
    // Check all components for connection points at this position
    for (ComponentGraphicsItem* comp : m_componentItems) {
        int terminalIndex = comp->getConnectionPointAt(pos);
        if (terminalIndex >= 0) {
            component = comp;
            terminal = terminalIndex;
            return true;
        }
    }
    
    return false;
}

QPointF CircuitCanvas::snapToGrid(const QPointF& pos) const
{
    if (!m_snapToGrid) {
        return pos;
    }
    
    qreal x = qRound(pos.x() / m_gridSize) * m_gridSize;
    qreal y = qRound(pos.y() / m_gridSize) * m_gridSize;
    
    return QPointF(x, y);
}

void CircuitCanvas::clearHighlights()
{
    for (ComponentGraphicsItem* comp : m_componentItems) {
        comp->setHighlighted(false);
    }
}

void CircuitCanvas::connectComponentSignals(ComponentGraphicsItem* component)
{
    connect(component, &ComponentGraphicsItem::connectionPointClicked,
            this, &CircuitCanvas::startWireDrawing);
    connect(component, &ComponentGraphicsItem::componentDoubleClicked,
            this, &CircuitCanvas::onComponentDoubleClicked);
    connect(component, &ComponentGraphicsItem::componentMoved,
            this, &CircuitCanvas::onComponentMoved);
}

void CircuitCanvas::removeWiresConnectedTo(ComponentGraphicsItem* component)
{
    // Find and remove all wires connected to this component
    for (int i = m_wireItems.size() - 1; i >= 0; --i) {
        WireGraphicsItem* wire = m_wireItems[i];
        if (wire->getStartComponent() == component || wire->getEndComponent() == component) {
            m_wireItems.removeAt(i);
            removeItem(wire);
            wire->deleteLater();
        }
    }
}

void CircuitCanvas::deleteSelectedItems()
{
    QList<QGraphicsItem*> selected = selectedItems();
    
    for (QGraphicsItem* item : selected) {
        ComponentGraphicsItem* comp = qgraphicsitem_cast<ComponentGraphicsItem*>(item);
        if (comp) {
            removeComponent(comp);
            continue;
        }
        
        WireGraphicsItem* wire = qgraphicsitem_cast<WireGraphicsItem*>(item);
        if (wire) {
            m_wireItems.removeOne(wire);
            removeItem(wire);
            wire->deleteLater();
            continue;
        }
    }
}

void CircuitCanvas::showContextMenu(const QPointF& scenePos, const QPoint& screenPos)
{
    QMenu contextMenu;
    
    // Add component options
    QMenu* addMenu = contextMenu.addMenu("Add Component");
    addMenu->addAction("LED", [this, scenePos]() {
        addLED(scenePos, Qt::red);
    });
    addMenu->addAction("Resistor", [this, scenePos]() {
        addResistor(scenePos, 1000.0);
    });
    addMenu->addAction("Arduino Uno", [this, scenePos]() {
        addArduino(scenePos, Arduino::UNO);
    });
    
    // View options
    contextMenu.addSeparator();
    QAction* gridAction = contextMenu.addAction("Show Grid");
    gridAction->setCheckable(true);
    gridAction->setChecked(m_showGrid);
    connect(gridAction, &QAction::triggered, this, &CircuitCanvas::toggleGrid);
    
    // Show menu
    contextMenu.exec(screenPos);
}

void CircuitCanvas::toggleGrid()
{
    m_showGrid = !m_showGrid;
    update(); // Redraw scene
}

// Slot implementations
void CircuitCanvas::onComponentDoubleClicked(ComponentGraphicsItem* component)
{
    qDebug() << "Component double-clicked:" << component->getComponentName();
    // Open component properties dialog
    emit componentPropertiesRequested(component);
}

void CircuitCanvas::onComponentMoved(ComponentGraphicsItem* component)
{
    Q_UNUSED(component)
    // Component movement is automatically handled by connected wires
    // through their connection to componentMoved signals
}

void CircuitCanvas::onWireDoubleClicked(WireGraphicsItem* wire)
{
    qDebug() << "Wire double-clicked";
    // Open wire properties dialog
    emit wirePropertiesRequested(wire);
}

void CircuitCanvas::onCircuitChanged()
{
    qDebug() << "Circuit changed - updating visual elements";
    // The circuit has changed - update any visual elements if needed
    // Most updates are handled automatically through component signals
}