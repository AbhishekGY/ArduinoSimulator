#include "simulation/Circuit.h"
#include "core/Component.h"
#include "core/Arduino.h"
#include "core/ArduinoPin.h"
#include "core/LED.h"
#include "core/Resistor.h"
#include "core/Wire.h"
#include "simulation/Node.h"
#include <QDebug>

// Updated constructor to initialize new members
Circuit::Circuit(QObject *parent)
    : QObject(parent)
    , m_simulator(nullptr)
    , m_simulationRunning(false)
    , m_groundNode(nullptr)
    , m_nodeCounter(1)
    , m_externalComponents()
{
    // Create ground node by default
    m_groundNode = createNode();
    m_groundNode->setAsGround(true);
    m_groundNode->setVoltage(0.0);
    m_namedNodes["GND"] = m_groundNode;
    m_namedNodes["GROUND"] = m_groundNode;
}

// Updated destructor to clean up wires
Circuit::~Circuit()
{
    qDeleteAll(m_components);
    qDeleteAll(m_nodes);
    qDeleteAll(m_wires);
}

// Existing methods (unchanged)
void Circuit::addComponent(Component *component)
{
    if (component && !m_components.contains(component)) {
        // If component belongs to Arduino, mark as external
        ArduinoPin* pin = qobject_cast<ArduinoPin*>(component);
        if (pin && pin->getArduino()) {
            m_externalComponents.insert(component);
        }
        
        m_components.append(component);
        component->setCircuit(this);
        emit circuitChanged();
    }
}

void Circuit::removeComponent(Component *component)
{
    removeComponentSafely(component);
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

void Circuit::onSimulationStep(int step, double time)
{
    // TODO: Implement simulation step
}

// NEW: Enhanced node management methods
Node* Circuit::findOrCreateNode(const QString& nodeName)
{
    if (!nodeName.isEmpty()) {
        // Check if named node exists
        if (m_namedNodes.contains(nodeName)) {
            return m_namedNodes[nodeName];
        }
        
        // Create new named node
        Node* node = createNode();
        m_namedNodes[nodeName] = node;
        qDebug() << "Created named node:" << nodeName;
        return node;
    }
    
    // Create anonymous node
    return createNode();
}

Node* Circuit::getGroundNode()
{
    return m_groundNode;
}

void Circuit::setGroundNode(Node* node)
{
    if (node && node != m_groundNode) {
        Node* oldGround = m_groundNode;
        m_groundNode = node;
        m_groundNode->setAsGround(true);
        m_groundNode->setVoltage(0.0);
        
        // Update named nodes
        m_namedNodes["GND"] = m_groundNode;
        m_namedNodes["GROUND"] = m_groundNode;
        
        // If old ground exists and is not used by components, remove it
        if (oldGround && oldGround->getConnections().isEmpty()) {
            removeNode(oldGround);
        }
        
        qDebug() << "Ground node changed to node" << m_groundNode->getId();
    }
}

// NEW: Component connection methods
bool Circuit::connectComponents(Component* comp1, int terminal1, 
                               Component* comp2, int terminal2)
{
    if (!comp1 || !comp2) {
        qWarning() << "Cannot connect null components";
        return false;
    }
    
    if (terminal1 < 0 || terminal1 >= comp1->getTerminalCount() ||
        terminal2 < 0 || terminal2 >= comp2->getTerminalCount()) {
        qWarning() << "Invalid terminal numbers for connection";
        return false;
    }
    
    // Check if components are already connected via the same terminals
    Node* node1 = comp1->getNode(terminal1);
    Node* node2 = comp2->getNode(terminal2);
    
    if (node1 && node2 && node1 == node2) {
        qDebug() << "Components already connected";
        return true;
    }
    
    // If neither component is connected to a node, create a new one
    if (!node1 && !node2) {
        Node* node = createNode();
        comp1->connectToNode(node, terminal1);
        comp2->connectToNode(node, terminal2);
        
        qDebug() << "Connected" << comp1->getName() << "terminal" << terminal1
                 << "to" << comp2->getName() << "terminal" << terminal2
                 << "via new node" << node->getId();
        return true;
    }
    
    // If one component is already connected, connect the other to the same node
    if (node1 && !node2) {
        comp2->connectToNode(node1, terminal2);
        qDebug() << "Connected" << comp2->getName() << "to existing node" << node1->getId();
        return true;
    }
    
    if (node2 && !node1) {
        comp1->connectToNode(node2, terminal1);
        qDebug() << "Connected" << comp1->getName() << "to existing node" << node2->getId();
        return true;
    }
    
    // Both components are connected to different nodes - merge the nodes
    if (node1 && node2 && node1 != node2) {
        // Move all connections from node2 to node1
        QVector<QPair<Component*, int>> connections = node2->getConnections();
        for (const auto& connection : connections) {
            connection.first->disconnectFromNode(connection.second);
            connection.first->connectToNode(node1, connection.second);
        }
        
        // Remove the now-empty node2
        removeNode(node2);
        
        qDebug() << "Merged nodes" << node1->getId() << "and" << node2->getId();
        return true;
    }
    
    return false;
}

bool Circuit::connectComponentToNode(Component* component, int terminal, Node* node)
{
    if (!component || !node) {
        qWarning() << "Cannot connect null component or node";
        return false;
    }
    
    if (terminal < 0 || terminal >= component->getTerminalCount()) {
        qWarning() << "Invalid terminal number:" << terminal;
        return false;
    }
    
    component->connectToNode(node, terminal);
    qDebug() << "Connected" << component->getName() << "terminal" << terminal
             << "to node" << node->getId();
    return true;
}

void Circuit::disconnectComponent(Component* component, int terminal)
{
    if (!component) {
        qWarning() << "Cannot disconnect null component";
        return;
    }
    
    if (terminal < 0 || terminal >= component->getTerminalCount()) {
        qWarning() << "Invalid terminal number:" << terminal;
        return;
    }
    
    Node* node = component->getNode(terminal);
    component->disconnectFromNode(terminal);
    
    if (node) {
        qDebug() << "Disconnected" << component->getName() << "terminal" << terminal
                 << "from node" << node->getId();
        
        // If node has no connections left and is not ground, remove it
        if (node->getConnections().isEmpty() && node != m_groundNode) {
            removeNode(node);
        }
    }
}

// NEW: Wire management
Wire* Circuit::addWire(Node* fromNode, Node* toNode)
{
    if (!fromNode || !toNode) {
        qWarning() << "Cannot create wire with null nodes";
        return nullptr;
    }
    
    if (fromNode == toNode) {
        qDebug() << "Wire connects node to itself - no wire needed";
        return nullptr;
    }
    
    // Check if wire already exists between these nodes
    for (Wire* wire : m_wires) {
        Node* wireFromNode = wire->getNode(0);
        Node* wireToNode = wire->getNode(1);
        
        if ((wireFromNode == fromNode && wireToNode == toNode) ||
            (wireFromNode == toNode && wireToNode == fromNode)) {
            qDebug() << "Wire already exists between nodes" << fromNode->getId() 
                     << "and" << toNode->getId();
            return wire;
        }
    }
    
    // Create new wire connecting the two nodes
    Wire* wire = Wire::createJumperWire(QPointF(), QPointF(), this);
    wire->connectToNode(fromNode, 0);
    wire->connectToNode(toNode, 1);
    
    m_wires.append(wire);
    addComponent(wire);
    
    qDebug() << "Created wire between nodes" << fromNode->getId() 
             << "and" << toNode->getId();
    return wire;
}

void Circuit::removeWire(Wire* wire)
{
    if (!wire) return;
    
    m_wires.removeOne(wire);
    removeComponent(wire);
    wire->deleteLater();
    
    qDebug() << "Removed wire";
}

// NEW: Arduino integration methods
bool Circuit::connectArduinoPin(Arduino* arduino, int pinNumber, Node* node)
{
    if (!arduino || !node) {
        qWarning() << "Cannot connect Arduino pin: null Arduino or node";
        return false;
    }
    
    ArduinoPin* pin = arduino->getPin(pinNumber);
    if (!pin) {
        // Try analog pin
        pin = arduino->getAnalogPin(pinNumber);
        if (!pin) {
            qWarning() << "Invalid Arduino pin number:" << pinNumber;
            return false;
        }
    }
    
    // Add Arduino pin to circuit if not already added
    if (!m_components.contains(pin)) {
        addComponent(pin);
    }
    
    // Arduino pins only have one terminal (terminal 0)
    pin->connectToNode(node, 0);
    
    qDebug() << "Connected Arduino pin" << pinNumber << "to node" << node->getId();
    return true;
}

bool Circuit::connectArduinoPinByName(Arduino* arduino, const QString& pinName, Node* node)
{
    if (!arduino || !node) {
        qWarning() << "Cannot connect Arduino pin: null Arduino or node";
        return false;
    }
    
    ArduinoPin* pin = nullptr;
    
    // Handle special pins
    if (pinName.toUpper() == "GND" || pinName.toUpper() == "GROUND") {
        pin = arduino->getGroundPin();
    } else if (pinName.toUpper() == "VCC" || pinName.toUpper() == "5V") {
        pin = arduino->getVccPin();
    } else if (pinName.startsWith("A", Qt::CaseInsensitive)) {
        // Analog pin (e.g., "A0", "A1")
        bool ok;
        int analogPin = pinName.mid(1).toInt(&ok);
        if (ok) {
            pin = arduino->getAnalogPin(analogPin);
        }
    } else if (pinName.startsWith("D", Qt::CaseInsensitive)) {
        // Digital pin (e.g., "D13", "D2")
        bool ok;
        int digitalPin = pinName.mid(1).toInt(&ok);
        if (ok) {
            pin = arduino->getPin(digitalPin);
        }
    } else {
        // Try parsing as number for digital pins
        bool ok;
        int digitalPin = pinName.toInt(&ok);
        if (ok) {
            pin = arduino->getPin(digitalPin);
        }
    }
    
    if (!pin) {
        qWarning() << "Could not find Arduino pin:" << pinName;
        return false;
    }
    
    // Add Arduino pin to circuit if not already added
    if (!m_components.contains(pin)) {
        addComponent(pin);
    }
    
    pin->connectToNode(node, 0);
    qDebug() << "Connected Arduino pin" << pinName << "to node" << node->getId();
    return true;
}

void Circuit::disconnectArduinoPin(Arduino* arduino, int pinNumber)
{
    if (!arduino) {
        qWarning() << "Cannot disconnect Arduino pin: null Arduino";
        return;
    }
    
    ArduinoPin* pin = arduino->getPin(pinNumber);
    if (!pin) {
        pin = arduino->getAnalogPin(pinNumber);
        if (!pin) {
            qWarning() << "Invalid Arduino pin number:" << pinNumber;
            return;
        }
    }
    
    pin->disconnectFromNode(0);
    qDebug() << "Disconnected Arduino pin" << pinNumber;
}

// NEW: Circuit validation methods
bool Circuit::validateConnections() const
{
    QStringList issues = getConnectionIssues();
    return issues.isEmpty();
}

QStringList Circuit::getConnectionIssues() const
{
    QStringList issues;
    
    // Check for floating nodes (nodes with only one connection)
    for (Node* node : m_nodes) {
        if (node->getConnections().size() < 2 && !node->isGroundNode()) {
            issues.append(QString("Node %1 has only one connection").arg(node->getId()));
        }
    }
    
    // Check for disconnected components
    for (Component* component : m_components) {
        bool hasConnection = false;
        for (int i = 0; i < component->getTerminalCount(); ++i) {
            if (component->getNode(i)) {
                hasConnection = true;
                break;
            }
        }
        if (!hasConnection) {
            issues.append(QString("Component %1 is not connected").arg(component->getName()));
        }
    }
    
    // Check if ground node exists
    if (!m_groundNode) {
        issues.append("No ground node found");
    }
    
    return issues;
}

// NEW: Helper methods
Node* Circuit::createVccNode(double voltage)
{
    Node* vccNode = findOrCreateNode("VCC");
    vccNode->setVoltage(voltage);
    qDebug() << "Created VCC node with" << voltage << "V";
    return vccNode;
}

bool Circuit::createSimpleArduinoLEDCircuit(Arduino* arduino, LED* led, Resistor* resistor)
{
    if (!arduino || !led || !resistor) {
        qWarning() << "Cannot create circuit: null components";
        return false;
    }
    
    // Add LED and resistor to circuit
    addComponent(led);
    addComponent(resistor);
    
    // Get Arduino pin 13 (typical LED pin)
    ArduinoPin* pin13 = arduino->getPin(13);
    ArduinoPin* groundPin = arduino->getGroundPin();
    
    if (!pin13 || !groundPin) {
        qWarning() << "Arduino pins not available";
        return false;
    }
    
    // Add Arduino pins as components
    addComponent(pin13);
    addComponent(groundPin);
    
    // Create circuit: Pin 13 -> LED -> Resistor -> Ground
    Node* pin13Node = createNode();
    Node* ledResistorNode = createNode();
    
    // Connect Arduino pin 13 to LED positive terminal
    connectComponentToNode(pin13, 0, pin13Node);
    connectComponentToNode(led, 0, pin13Node); // LED anode
    
    // Connect LED negative terminal to resistor
    connectComponentToNode(led, 1, ledResistorNode); // LED cathode
    connectComponentToNode(resistor, 0, ledResistorNode);
    
    // Connect resistor to ground
    connectComponentToNode(resistor, 1, m_groundNode);
    
    // Connect Arduino ground pin
    connectComponentToNode(groundPin, 0, m_groundNode);
    
    qDebug() << "Created simple Arduino LED circuit with pin 13";
    return true;
}

bool Circuit::isComponentInCircuit(Component* component) const
{
    return m_components.contains(component);
}

void Circuit::disconnectComponent(Component* component)
{
    if (!component) return;
    
    // Disconnect from all connected nodes
    for (int i = 0; i < component->getTerminalCount(); i++) {
        Node* node = component->getNode(i);
        if (node) {
            component->disconnectFromNode(i);
        }
    }
}

void Circuit::removeComponentSafely(Component* component)
{
    if (!component) return;
    
    // First disconnect from all nodes
    disconnectComponent(component);
    
    // Then remove from components list
    if (m_components.removeOne(component)) {
        component->setCircuit(nullptr);
        
        // If this is not an external component, delete it
        if (!m_externalComponents.contains(component)) {
            component->deleteLater();
        } else {
            m_externalComponents.remove(component);
        }
        
        emit circuitChanged();
    }
}

void Circuit::removeAllComponents()
{
    // Make a copy of components list since we'll be modifying it
    QVector<Component*> componentsCopy = m_components;
    
    // Remove each component safely
    for (Component* component : componentsCopy) {
        removeComponentSafely(component);
    }
    
    // Make sure lists are clear
    m_components.clear();
    m_externalComponents.clear();
}

void Circuit::clearArduinoConnections(Arduino* arduino)
{
    if (!arduino) return;
    
    QVector<ArduinoPin*> allPins = arduino->getAllPins();
    for (ArduinoPin* pin : allPins) {
        if (pin && isComponentInCircuit(pin)) {
            // Mark as external so we don't delete it
            m_externalComponents.insert(pin);
            // Disconnect and remove from circuit
            removeComponentSafely(pin);
        }
    }
}