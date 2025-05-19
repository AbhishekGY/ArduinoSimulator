#include "core/ElectricalComponent.h"
#include "simulation/Node.h"

ElectricalComponent::ElectricalComponent(const QString &name, int terminalCount, QObject *parent)
    : Component(name, parent)
    , m_terminals(terminalCount, nullptr)
    , m_voltage(0.0)
    , m_current(0.0)
{
}

void ElectricalComponent::connectToNode(Node *node, int terminal)
{
    if (terminal >= 0 && terminal < m_terminals.size()) {
        m_terminals[terminal] = node;
        if (node) {
            node->addComponent(this, terminal);
        }
    }
}

void ElectricalComponent::disconnectFromNode(int terminal)
{
    if (terminal >= 0 && terminal < m_terminals.size() && m_terminals[terminal]) {
        m_terminals[terminal]->removeComponent(this);
        m_terminals[terminal] = nullptr;
    }
}

Node *ElectricalComponent::getNode(int terminal) const
{
    if (terminal >= 0 && terminal < m_terminals.size()) {
        return m_terminals[terminal];
    }
    return nullptr;
}

void ElectricalComponent::updateState(double voltage, double current)
{
    m_voltage = voltage;
    m_current = current;
    emit stateChanged(voltage, current);
}

void ElectricalComponent::reset()
{
    m_voltage = 0.0;
    m_current = 0.0;
    emit stateChanged(m_voltage, m_current);
}
