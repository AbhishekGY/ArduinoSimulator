#include "simulation/CircuitSimulator.h"
#include "simulation/Circuit.h"
#include "simulation/MatrixSolver.h"
#include "simulation/Node.h"
#include "core/Component.h"
#include "core/ElectricalComponent.h"
#include <QDebug>
#include <algorithm>
#include <QtMath>

CircuitSimulator::CircuitSimulator(Circuit *circuit, QObject *parent)
    : QObject(parent)
    , m_circuit(circuit)
    , m_matrixSolver(new MatrixSolver(this))
    , m_maxIterations(100)
    , m_convergenceTolerance(1e-6)
    , m_timeStep(0.001)
    , m_minUpdateIntervalMs(10)
    , m_running(false)
    , m_initialized(false)
    , m_iterationCount(0)
    , m_simulationTime(0.0)
    , m_isUpdating(false)
    , m_updatePending(false)
{
    // Initialize update timer
    m_updateTimer.setSingleShot(true);
    connect(&m_updateTimer, &QTimer::timeout, this, &CircuitSimulator::doUpdate);
    
    m_lastUpdateTime.start();
    
    // Connect to circuit signals
    if (m_circuit) {
        connect(m_circuit, &Circuit::circuitChanged, 
                this, &CircuitSimulator::onCircuitChanged);
                
        // Connect simulator signals back to circuit slots (assuming the circuit has corresponding slots)
        connect(this, &CircuitSimulator::simulationStarted, 
                m_circuit, &Circuit::startSimulation);
        connect(this, &CircuitSimulator::simulationStopped, 
                m_circuit, &Circuit::stopSimulation);
        connect(this, &CircuitSimulator::simulationStepCompleted, 
                m_circuit, &Circuit::onSimulationStep);
    }
}

CircuitSimulator::~CircuitSimulator()
{
    stop();
}

bool CircuitSimulator::initialize()
{
    if (!m_circuit) {
        emit simulationError("No circuit to simulate");
        return false;
    }
    
    qDebug() << "Initializing circuit simulation";
    
    // Assign IDs to nodes for matrix indexing
    assignNodeIds();
    
    // Clear previous state
    m_prevValues.clear();
    
    // Initialize matrix solver
    int nodeCount = getNodeCount();
    if (nodeCount <= 0) {
        emit simulationError("Circuit has no nodes to simulate");
        return false;
    }
    
    m_matrixSolver->setDimension(nodeCount);
    m_matrixSolver->clear();
    
    // Store initial component values
    const QVector<Component*> &components = m_circuit->getComponents();
    for (Component* comp : components) {
        ElectricalComponent* elecComp = qobject_cast<ElectricalComponent*>(comp);
        if (elecComp) {
            m_prevValues[elecComp] = qMakePair(elecComp->getVoltage(), elecComp->getCurrent());
        }
    }
    
    m_initialized = true;
    m_iterationCount = 0;
    m_simulationTime = 0.0;
    
    return true;
}

bool CircuitSimulator::solve()
{
    QMutexLocker locker(&m_simulationMutex);
    
    if (!m_initialized) {
        if (!initialize()) {
            return false;
        }
    }
    
    // Start a new simulation step
    m_iterationCount = 0;
    bool converged = false;
    
    // Iterative solving for non-linear components
    while (m_iterationCount < m_maxIterations && !converged) {
        // Build system matrices
        if (!buildMatrices()) {
            emit simulationError("Failed to build circuit matrices");
            return false;
        }
        
        // Solve the system
        if (!m_matrixSolver->solve()) {
            emit simulationError("Failed to solve circuit equations");
            return false;
        }
        
        // Update component states
        if (!updateComponentStates()) {
            emit simulationError("Failed to update component states");
            return false;
        }
        
        // Check for convergence
        converged = hasConverged();
        
        // Increment iteration counter
        m_iterationCount++;
    }
    
    // Check if convergence was achieved
    if (converged) {
        emit convergenceAchieved();
    } else {
        emit convergenceFailed(m_iterationCount);
    }
    
    // Update simulation time
    m_simulationTime += m_timeStep;
    
    // Emit step completed signal
    emit simulationStepCompleted(m_iterationCount, m_simulationTime);
    
    return converged;
}

void CircuitSimulator::start()
{
    if (m_running) return;
    
    // Initialize the simulation
    if (!m_initialized) {
        if (!initialize()) {
            return;
        }
    }
    
    m_running = true;
    emit simulationStarted();
    
    // Perform initial solve
    triggerUpdate();
}

void CircuitSimulator::stop()
{
    if (!m_running) return;
    
    m_running = false;
    m_updateTimer.stop();
    m_updatePending = false;
    
    emit simulationStopped();
}

void CircuitSimulator::reset()
{
    QMutexLocker locker(&m_simulationMutex);
    
    // Reset all components to initial state
    const QVector<Component*> &components = m_circuit->getComponents();
    for (Component* comp : components) {
        comp->reset();
    }
    
    // Reset simulation state
    m_initialized = false;
    m_iterationCount = 0;
    m_simulationTime = 0.0;
    
    emit simulationReset();
}

void CircuitSimulator::step()
{
    if (!m_running) {
        // Initialize if needed
        if (!m_initialized) {
            if (!initialize()) {
                return;
            }
        }
    }
    
    // Perform a single simulation step
    solve();
}

void CircuitSimulator::triggerUpdate()
{
    // Prevent recursive simulation
    if (m_isUpdating) {
        // Already in an update cycle, do nothing
        qDebug() << "Ignoring recursive simulation update request";
        return;
    }
    
    // Throttle updates to avoid excessive calculations
    if (m_lastUpdateTime.elapsed() < m_minUpdateIntervalMs) {
        // If update is already pending, do nothing
        if (!m_updatePending) {
            int delay = m_minUpdateIntervalMs - m_lastUpdateTime.elapsed();
            m_updateTimer.start(std::max(1, delay));
            m_updatePending = true;
            qDebug() << "Throttling simulation update, scheduled in" << delay << "ms";
        }
        return;
    }
    
    // Start immediate update
    doUpdate();
}

void CircuitSimulator::doUpdate()
{
    QMutexLocker locker(&m_simulationMutex);
    
    m_isUpdating = true;
    m_updatePending = false;
    m_lastUpdateTime.restart();
    
    if (m_running) {
        // Solve the circuit equations
        solve();
    }
    
    m_isUpdating = false;
}

void CircuitSimulator::onComponentChanged()
{
    // A component has changed state - trigger a simulation update
    if (m_running) {
        qDebug() << "Component state changed, triggering simulation update";
        triggerUpdate();
    }
}

void CircuitSimulator::onCircuitChanged()
{
    // Circuit topology has changed - reinitialize and trigger update
    qDebug() << "Circuit topology changed, reinitializing simulation";
    m_initialized = false;
    
    if (m_running) {
        triggerUpdate();
    }
}

bool CircuitSimulator::buildMatrices()
{
    if (!m_circuit || !m_matrixSolver) {
        return false;
    }
    
    // Clear the matrix for a fresh build
    m_matrixSolver->clear();
    
    // Process each electrical component
    const QVector<Component*> &components = m_circuit->getComponents();
    for (Component* comp : components) {
        ElectricalComponent* elecComp = qobject_cast<ElectricalComponent*>(comp);
        if (!elecComp) continue;
        
        // Get component resistance/conductance
        double resistance = elecComp->getResistance();
        if (resistance <= 0.0) {
            // For ideal voltage sources, use a very small resistance
            // to avoid division by zero
            resistance = 1e-6;
        }
        double conductance = 1.0 / resistance;
        
        // Process based on terminal count
        int terminalCount = elecComp->getTerminalCount();
        
        if (terminalCount == 1) {
            // Single terminal components (e.g., Arduino pins)
            Node* node = elecComp->getNode(0);
            if (!node) continue;
            
            int nodeIndex = m_nodeIndices.value(node, -1);
            if (nodeIndex < 0) continue;
            
            // Special handling for Arduino pins
            // ArduinoPin* pin = qobject_cast<ArduinoPin*>(elecComp);
            // if (pin && pin->isOutput()) {
            //     // For output pins, add as voltage source
            //     m_matrixSolver->addVoltageSource(nodeIndex, -1, pin->getVoltage());
            // } else {
                // Add as conductance to ground
                m_matrixSolver->addConductance(nodeIndex, -1, conductance);
            // }
        }
        else if (terminalCount == 2) {
            // Two terminal components (e.g., resistors, LEDs)
            Node* node1 = elecComp->getNode(0);
            Node* node2 = elecComp->getNode(1);
            
            if (!node1 || !node2) continue;
            
            int nodeIndex1 = m_nodeIndices.value(node1, -1);
            int nodeIndex2 = m_nodeIndices.value(node2, -1);
            
            if (nodeIndex1 < 0 || nodeIndex2 < 0) continue;
            
            // Add conductance between the two nodes
            m_matrixSolver->addConductance(nodeIndex1, nodeIndex2, conductance);
        }
    }
    
    return true;
}

bool CircuitSimulator::updateComponentStates()
{
    if (!m_circuit || !m_matrixSolver) {
        return false;
    }
    
    // Process each electrical component
    const QVector<Component*> &components = m_circuit->getComponents();
    for (Component* comp : components) {
        ElectricalComponent* elecComp = qobject_cast<ElectricalComponent*>(comp);
        if (!elecComp) continue;
        
        // Process based on terminal count
        int terminalCount = elecComp->getTerminalCount();
        
        if (terminalCount == 1) {
            // Single terminal components (e.g., Arduino pins)
            Node* node = elecComp->getNode(0);
            if (!node) continue;
            
            int nodeIndex = m_nodeIndices.value(node, -1);
            if (nodeIndex < 0) continue;
            
            // Get the node voltage
            double voltage = m_matrixSolver->getNodeVoltage(nodeIndex);
            
            // Calculate current (voltage / resistance)
            double current = voltage / elecComp->getResistance();
            
            // Update component with new values but don't trigger recursive update
            elecComp->updateState(voltage, current);
        }
        else if (terminalCount == 2) {
            // Two terminal components (e.g., resistors, LEDs)
            Node* node1 = elecComp->getNode(0);
            Node* node2 = elecComp->getNode(1);
            
            if (!node1 || !node2) continue;
            
            int nodeIndex1 = m_nodeIndices.value(node1, -1);
            int nodeIndex2 = m_nodeIndices.value(node2, -1);
            
            if (nodeIndex1 < 0 || nodeIndex2 < 0) continue;
            
            // Get the voltage across the component
            double v1 = m_matrixSolver->getNodeVoltage(nodeIndex1);
            double v2 = m_matrixSolver->getNodeVoltage(nodeIndex2);
            double voltage = v1 - v2;
            
            // Get the current through the component
            double current = m_matrixSolver->getBranchCurrent(nodeIndex1, nodeIndex2);
            
            // Update component with new values but don't trigger recursive update
            elecComp->updateState(voltage, current);
        }
    }
    
    return true;
}

bool CircuitSimulator::hasConverged()
{
    if (m_prevValues.isEmpty()) return false;
    
    // Check each electrical component for change in voltage/current
    const QVector<Component*> &components = m_circuit->getComponents();
    for (Component* comp : components) {
        ElectricalComponent* elecComp = qobject_cast<ElectricalComponent*>(comp);
        if (!elecComp) continue;
        
        // Get previous values
        auto prevIt = m_prevValues.find(elecComp);
        if (prevIt == m_prevValues.end()) {
            // No previous value - add current values and consider not converged
            m_prevValues[elecComp] = qMakePair(elecComp->getVoltage(), elecComp->getCurrent());
            return false;
        }
        
        double prevVoltage = prevIt.value().first;
        double prevCurrent = prevIt.value().second;
        
        // Get current values
        double voltage = elecComp->getVoltage();
        double current = elecComp->getCurrent();
        
        // Check if change is greater than tolerance
        double voltageDiff = qAbs(voltage - prevVoltage);
        double currentDiff = qAbs(current - prevCurrent);
        
        if (voltageDiff > m_convergenceTolerance || 
            currentDiff > m_convergenceTolerance) {
            
            // Update previous values for next iteration
            m_prevValues[elecComp] = qMakePair(voltage, current);
            return false;
        }
    }
    
    return true;
}

void CircuitSimulator::assignNodeIds()
{
    m_nodeIndices.clear();
    
    if (!m_circuit) return;
    
    const QVector<Node*> &nodes = m_circuit->getNodes();
    
    // Find ground node first
    Node* groundNode = m_circuit->getGroundNode();
    if (groundNode) {
        // Ground node is always index 0
        m_nodeIndices[groundNode] = 0;
    }
    
    // Assign indices to all other nodes
    int nextIndex = (groundNode) ? 1 : 0;
    for (Node* node : nodes) {
        if (node != groundNode) {
            m_nodeIndices[node] = nextIndex++;
        }
    }
    
    qDebug() << "Assigned indices to" << m_nodeIndices.size() << "nodes";
}

int CircuitSimulator::getNodeCount() const
{
    return m_nodeIndices.size();
}