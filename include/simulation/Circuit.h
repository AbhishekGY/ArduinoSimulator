#ifndef CIRCUIT_H
#define CIRCUIT_H

#include <QObject>
#include <QVector>
#include <QHash>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QSet>

class Component;
class Node;
class CircuitSimulator;
class Arduino;
class LED;
class Resistor;
class Wire;

class Circuit : public QObject
{
    Q_OBJECT

public:
    explicit Circuit(QObject *parent = nullptr);
    ~Circuit();

    // Component management (existing)
    void addComponent(Component *component);
    void removeComponent(Component *component);
    const QVector<Component*> &getComponents() const { return m_components; }

    // Node management (existing + new)
    Node *createNode();
    void removeNode(Node *node);
    const QVector<Node*> &getNodes() const { return m_nodes; }
    
    // NEW: Enhanced node management
    Node* findOrCreateNode(const QString& nodeName = QString());
    Node* getGroundNode();
    void setGroundNode(Node* node);
    
    // NEW: Component connection methods
    bool connectComponents(Component* comp1, int terminal1, 
                          Component* comp2, int terminal2);
    bool connectComponentToNode(Component* component, int terminal, Node* node);
    void disconnectComponent(Component* component, int terminal);
    
    // NEW: Wire management  
    Wire* addWire(Node* fromNode, Node* toNode);
    void removeWire(Wire* wire);
    const QVector<Wire*>& getWires() const { return m_wires; }
    
    // NEW: Arduino integration
    bool connectArduinoPin(Arduino* arduino, int pinNumber, Node* node);
    bool connectArduinoPinByName(Arduino* arduino, const QString& pinName, Node* node);
    void disconnectArduinoPin(Arduino* arduino, int pinNumber);
    
    // NEW: Circuit validation
    bool validateConnections() const;
    QStringList getConnectionIssues() const;
    
    // NEW: Helper methods for Arduino circuits
    Node* createVccNode(double voltage = 5.0);
    bool createSimpleArduinoLEDCircuit(Arduino* arduino, LED* led, Resistor* resistor);

    // Simulation (existing)
    void startSimulation();
    void stopSimulation();
    bool isSimulationRunning() const { return m_simulationRunning; }

    // Circuit changes (existing)
    void componentChanged(Component *component);

    // Safety methods for component management
    bool isComponentInCircuit(Component* component) const;
    void disconnectComponent(Component* component);
    void removeComponentSafely(Component* component);
    void removeAllComponents();
    void clearArduinoConnections(Arduino* arduino);

signals:
    void circuitChanged();

public slots:
    void simulationStarted();
    void simulationStopped();
    void onSimulationStep(int step, double time);

private:
    // Existing members
    QVector<Component*> m_components;
    QVector<Node*> m_nodes;
    CircuitSimulator *m_simulator;
    bool m_simulationRunning;
    QMutex m_circuitMutex;
    
    // NEW: Additional members for connection management
    QVector<Wire*> m_wires;
    Node* m_groundNode;
    QHash<QString, Node*> m_namedNodes;  // For ground, VCC, etc.
    int m_nodeCounter;
    QSet<Component*> m_externalComponents;
};

#endif // CIRCUIT_H