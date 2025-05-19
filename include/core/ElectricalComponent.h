#ifndef ELECTRICALCOMPONENT_H
#define ELECTRICALCOMPONENT_H

#include "Component.h"
#include <QVector>

class Node;

class ElectricalComponent : public Component
{
    Q_OBJECT

public:
    explicit ElectricalComponent(const QString &name, int terminalCount = 2, QObject *parent = nullptr);

    // Electrical properties
    virtual double getResistance() const = 0;
    virtual double getVoltage() const { return m_voltage; }
    virtual double getCurrent() const { return m_current; }

    // Node connections
    void connectToNode(Node *node, int terminal) override;
    void disconnectFromNode(int terminal) override;
    Node *getNode(int terminal) const override;
    int getTerminalCount() const override { return m_terminals.size(); }

    // Simulation updates
    virtual void updateState(double voltage, double current);

    void reset() override;

signals:
    void stateChanged(double voltage, double current);

protected:
    QVector<Node*> m_terminals;
    double m_voltage;
    double m_current;
};

#endif // ELECTRICALCOMPONENT_H
