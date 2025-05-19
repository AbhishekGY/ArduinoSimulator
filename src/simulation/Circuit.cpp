#include "simulation/Circuit.h"
#include "core/Component.h"
#include "simulation/Node.h"

Circuit::Circuit(QObject *parent)
    : QObject(parent)
    , m_simulator(nullptr)
    , m_simulationRunning(false)
{
}

Circuit::~Circuit()
{
    // Clean up components and nodes
    qDeleteAll(m_components);
    qDeleteAll(m_nodes);
}

void Circuit::addComponent(Component *component)
{
    if (component && !m_components.contains(component)) {
        m_components.append(component);
        component->setCircuit(this);
        emit circuitChanged();
    }
}

void Circuit::removeComponent(Component *component)
{
    if (m_components.removeOne(component)) {
        component->setCircuit(nullptr);
        emit circuitChanged();
    }
}

Node *Circuit::createNode()
{
    Node *node = new Node(this);
    m_nodes.append(node);
    return node;
}

void Circuit::removeNode(Node *node)
{
    if (m_nodes.removeOne(node)) {
        node->deleteLater();
    }
}

void Circuit::componentChanged(Component *component)
{
    Q_UNUSED(component)
    // TODO: Trigger simulation update
    emit circuitChanged();
}

void Circuit::startSimulation()
{
    // TODO: Implement simulation start
    m_simulationRunning = true;
    emit simulationStarted();
}

void Circuit::stopSimulation()
{
    // TODO: Implement simulation stop
    m_simulationRunning = false;
    emit simulationStopped();
}

void Circuit::onSimulationStep()
{
    // TODO: Implement simulation step
}
