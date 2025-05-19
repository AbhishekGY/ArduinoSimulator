#include "simulation/Node.h"
#include "core/Component.h"

int Node::s_nextId = 1;

Node::Node(QObject *parent)
    : QObject(parent)
    , m_id(s_nextId++)
    , m_voltage(0.0)
    , m_isGround(false)
{
}

void Node::addComponent(Component *component, int terminal)
{
    m_connections.append(qMakePair(component, terminal));
}

void Node::removeComponent(Component *component)
{
    for (int i = m_connections.size() - 1; i >= 0; --i) {
        if (m_connections[i].first == component) {
            m_connections.removeAt(i);
        }
    }
}

void Node::setVoltage(double voltage)
{
    if (m_voltage != voltage) {
        m_voltage = voltage;
        emit voltageChanged(voltage);
    }
}
