#ifndef NODE_H
#define NODE_H

#include <QObject>
#include <QVector>
#include <QPair>

class Component;

class Node : public QObject
{
    Q_OBJECT

public:
    explicit Node(QObject *parent = nullptr);

    // Component connections
    void addComponent(Component *component, int terminal);
    void removeComponent(Component *component);
    const QVector<QPair<Component*, int>> &getConnections() const { return m_connections; }

    // Electrical properties
    double getVoltage() const { return m_voltage; }
    void setVoltage(double voltage);

    // Node properties
    bool isGroundNode() const { return m_isGround; }
    void setAsGround(bool isGround) { m_isGround = isGround; }

    int getId() const { return m_id; }

signals:
    void voltageChanged(double voltage);

private:
    static int s_nextId;
    int m_id;
    double m_voltage;
    bool m_isGround;
    QVector<QPair<Component*, int>> m_connections;
};

#endif // NODE_H
