#ifndef COMPONENT_H
#define COMPONENT_H

#include <QObject>
#include <QString>
#include <QUuid>

class Circuit;
class Node;

class Component : public QObject
{
    Q_OBJECT

public:
    explicit Component(const QString &name, QObject *parent = nullptr);
    virtual ~Component() = default;

    // Basic properties
    const QUuid &getId() const { return m_id; }
    const QString &getName() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    // Circuit integration
    virtual void setCircuit(Circuit *circuit) { m_circuit = circuit; }
    Circuit *getCircuit() const { return m_circuit; }

    // Node connections
    virtual void connectToNode(Node *node, int terminal) = 0;
    virtual void disconnectFromNode(int terminal) = 0;
    virtual Node *getNode(int terminal) const = 0;
    virtual int getTerminalCount() const = 0;

    // Component behavior
    virtual void reset() = 0;

signals:
    void componentChanged();

protected:
    QUuid m_id;
    QString m_name;
    Circuit *m_circuit;
};

#endif // COMPONENT_H
