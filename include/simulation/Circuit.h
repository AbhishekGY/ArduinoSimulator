#ifndef CIRCUIT_H
#define CIRCUIT_H

#include <QObject>
#include <QVector>
#include <QHash>
#include <QMutex>
#include <QSet>

class Component;
class Node;
class CircuitSimulator;
class Arduino;
class ArduinoPin;
class LED;
class Resistor;
class Wire;

class Circuit : public QObject
{
    Q_OBJECT

public:
    explicit Circuit(QObject *parent = nullptr);
    ~Circuit();

    // Component management
    void addComponent(Component *component);
    void removeComponent(Component *component);
    const QVector<Component*> &getComponents() const { return m_components; }

    // Node management
    Node *createNode();
    void removeNode(Node *node);
    const QVector<Node*> &getNodes() const { return m_nodes; }

    // Enhanced node management
    Node* findOrCreateNode(const QString& nodeName);
    Node* getGroundNode();
    void setGroundNode(Node* node);

    // Component connection methods
    bool connectComponents(Component* comp1, int terminal1, 
                          Component* comp2, int terminal2);
    bool connectComponentToNode(Component* component, int terminal, Node* node);
    void disconnectComponent(Component* component, int terminal);
    void disconnectComponent(Component* component); // Disconnect all terminals

    // Wire management
    Wire* addWire(Node* fromNode, Node* toNode);
    void removeWire(Wire* wire);
    const QVector<Wire*> &getWires() const { return m_wires; }

    // Arduino integration methods
    bool connectArduinoPin(Arduino* arduino, int pinNumber, Node* node);
    bool connectArduinoPinByName(Arduino* arduino, const QString& pinName, Node* node);
    void disconnectArduinoPin(Arduino* arduino, int pinNumber);

    // Circuit validation
    bool validateConnections() const;
    QStringList getConnectionIssues() const;

    // Helper methods
    Node* createVccNode(double voltage = 5.0);
    bool createSimpleArduinoLEDCircuit(Arduino* arduino, LED* led, Resistor* resistor);
    bool isComponentInCircuit(Component* component) const;

    // Component cleanup
    void removeComponentSafely(Component* component);
    void removeAllComponents();
    void clearArduinoConnections(Arduino* arduino);

    // Simulation status
    bool isSimulationRunning() const { return m_simulationRunning; }

    // Circuit changes
    void componentChanged(Component *component);

    // Simulator integration
    void setSimulator(CircuitSimulator* simulator) { m_simulator = simulator; }
    CircuitSimulator* getSimulator() const { return m_simulator; }

public slots:
    // Simulation control slots (called by CircuitSimulator)
    void startSimulation();
    void stopSimulation();
    void onSimulationStep(int step, double time);

signals:
    void circuitChanged();

private:
    QVector<Component*> m_components;
    QVector<Node*> m_nodes;
    QVector<Wire*> m_wires;
    CircuitSimulator *m_simulator;
    bool m_simulationRunning;
    QMutex m_circuitMutex;

    // Enhanced node management
    Node* m_groundNode;
    int m_nodeCounter;
    QHash<QString, Node*> m_namedNodes;

    // External component tracking (components owned by other objects like Arduino)
    QSet<Component*> m_externalComponents;
};

#endif // CIRCUIT_H