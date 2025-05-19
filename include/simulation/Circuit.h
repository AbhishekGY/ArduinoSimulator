#ifndef CIRCUIT_H
#define CIRCUIT_H

#include <QObject>
#include <QVector>
#include <QHash>
#include <QMutex>

class Component;
class Node;
class CircuitSimulator;

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

    // Simulation
    void startSimulation();
    void stopSimulation();
    bool isSimulationRunning() const { return m_simulationRunning; }

    // Circuit changes
    void componentChanged(Component *component);

signals:
    void simulationStarted();
    void simulationStopped();
    void circuitChanged();

private slots:
    void onSimulationStep();

private:
    QVector<Component*> m_components;
    QVector<Node*> m_nodes;
    CircuitSimulator *m_simulator;
    bool m_simulationRunning;
    QMutex m_circuitMutex;
};

#endif // CIRCUIT_H
