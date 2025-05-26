#include "simulation/CircuitSimulator.h"
#include "simulation/Circuit.h"
#include "simulation/MatrixSolver.h"
#include "simulation/Node.h"
#include "core/Component.h"
#include "core/ElectricalComponent.h"
#include "core/ArduinoPin.h"
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
    qDebug() << "DEBUG: CircuitSimulator constructor starting";
    
    // Initialize update timer
    m_updateTimer.setSingleShot(true);
    connect(&m_updateTimer, &QTimer::timeout, this, &CircuitSimulator::doUpdate);
    
    m_lastUpdateTime.start();
    
    // Connect to circuit signals
    if (m_circuit) {
        qDebug() << "DEBUG: Connecting circuit signals";
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
    
    qDebug() << "DEBUG: CircuitSimulator constructor completed";
}

CircuitSimulator::~CircuitSimulator()
{
    qDebug() << "DEBUG: CircuitSimulator destructor";
    stop();
}

bool CircuitSimulator::initialize()
{
    qDebug() << "DEBUG: CircuitSimulator::initialize() starting";
    
    if (!m_circuit) {
        qDebug() << "DEBUG: No circuit to simulate";
        emit simulationError("No circuit to simulate");
        return false;
    }
    
    qDebug() << "Initializing circuit simulation";
    
    // Assign IDs to nodes for matrix indexing
    qDebug() << "DEBUG: Assigning node IDs";
    assignNodeIds();
    
    // Clear previous state
    qDebug() << "DEBUG: Clearing previous values";
    m_prevValues.clear();
    
    // Initialize matrix solver
    int nodeCount = getNodeCount();
    qDebug() << "DEBUG: Node count:" << nodeCount;
    
    if (nodeCount <= 0) {
        qDebug() << "DEBUG: Circuit has no nodes to simulate";
        emit simulationError("Circuit has no nodes to simulate");
        return false;
    }
    
    qDebug() << "DEBUG: Setting matrix solver dimension to" << nodeCount;
    m_matrixSolver->setDimension(nodeCount);
    m_matrixSolver->clear();
    
    // Store initial component values
    const QVector<Component*> &components = m_circuit->getComponents();
    qDebug() << "DEBUG: Storing initial values for" << components.size() << "components";
    
    for (Component* comp : components) {
        ElectricalComponent* elecComp = qobject_cast<ElectricalComponent*>(comp);
        if (elecComp) {
            m_prevValues[elecComp] = qMakePair(elecComp->getVoltage(), elecComp->getCurrent());
            qDebug() << "DEBUG: Initial values for" << elecComp->getName() 
                     << "V:" << elecComp->getVoltage() << "I:" << elecComp->getCurrent();
        }
    }
    
    m_initialized = true;
    m_iterationCount = 0;
    m_simulationTime = 0.0;
    
    qDebug() << "DEBUG: CircuitSimulator::initialize() completed successfully";
    return true;
}

bool CircuitSimulator::solve()
{
    qDebug() << "DEBUG: CircuitSimulator::solve() starting";
    
    // Note: No mutex lock here since solve() is always called from functions 
    // that already hold the mutex (doUpdate, step)
    
    if (!m_initialized) {
        qDebug() << "DEBUG: Not initialized, calling initialize()";
        if (!initialize()) {
            qDebug() << "DEBUG: Initialize failed";
            return false;
        }
    }
    
    // Start a new simulation step
    m_iterationCount = 0;
    bool converged = false;
    
    qDebug() << "DEBUG: Starting iteration loop, max iterations:" << m_maxIterations;
    
    // Iterative solving for non-linear components
    while (m_iterationCount < m_maxIterations && !converged) {
        qDebug() << "DEBUG: Iteration" << m_iterationCount + 1 << "of" << m_maxIterations;
        
        // Build system matrices
        qDebug() << "DEBUG: Building matrices";
        if (!buildMatrices()) {
            qDebug() << "DEBUG: Failed to build circuit matrices";
            emit simulationError("Failed to build circuit matrices");
            return false;
        }
        qDebug() << "DEBUG: Matrices built successfully";
        
        // Solve the system
        qDebug() << "DEBUG: Solving matrix system";
        if (!m_matrixSolver->solve()) {
            qDebug() << "DEBUG: Failed to solve circuit equations";
            emit simulationError("Failed to solve circuit equations");
            return false;
        }
        qDebug() << "DEBUG: Matrix system solved successfully";
        
        // Update component states
        qDebug() << "DEBUG: Updating component states";
        if (!updateComponentStates()) {
            qDebug() << "DEBUG: Failed to update component states";
            emit simulationError("Failed to update component states");
            return false;
        }
        qDebug() << "DEBUG: Component states updated successfully";
        
        // Check for convergence
        qDebug() << "DEBUG: Checking convergence";
        converged = hasConverged();
        qDebug() << "DEBUG: Convergence check result:" << (converged ? "CONVERGED" : "NOT CONVERGED");
        
        // Increment iteration counter
        m_iterationCount++;
        qDebug() << "DEBUG: Completed iteration" << m_iterationCount << "converged:" << converged;
    }
    
    // Check if convergence was achieved
    if (converged) {
        qDebug() << "DEBUG: ✓ Simulation converged after" << m_iterationCount << "iterations";
        emit convergenceAchieved();
    } else {
        qDebug() << "DEBUG: ✗ Simulation failed to converge after" << m_iterationCount << "iterations";
        emit convergenceFailed(m_iterationCount);
    }
    
    // Update simulation time
    m_simulationTime += m_timeStep;
    
    // Emit step completed signal
    qDebug() << "DEBUG: Emitting simulationStepCompleted signal";
    emit simulationStepCompleted(m_iterationCount, m_simulationTime);
    
    qDebug() << "DEBUG: CircuitSimulator::solve() completed, result:" << converged;
    return converged;
}

void CircuitSimulator::start()
{
    qDebug() << "DEBUG: CircuitSimulator::start() called";
    
    if (m_running) {
        qDebug() << "DEBUG: Already running, ignoring start()";
        return;
    }
    
    // Initialize the simulation
    if (!m_initialized) {
        qDebug() << "DEBUG: Not initialized, calling initialize()";
        if (!initialize()) {
            qDebug() << "DEBUG: Initialize failed in start()";
            return;
        }
    }
    
    m_running = true;
    qDebug() << "DEBUG: Set running to true, emitting simulationStarted";
    emit simulationStarted();
    
    // Perform initial solve
    qDebug() << "DEBUG: Triggering initial update";
    triggerUpdate();
    qDebug() << "DEBUG: CircuitSimulator::start() completed";
}

void CircuitSimulator::stop()
{
    qDebug() << "DEBUG: CircuitSimulator::stop() called";
    
    if (!m_running) {
        qDebug() << "DEBUG: Not running, ignoring stop()";
        return;
    }
    
    m_running = false;
    m_updateTimer.stop();
    m_updatePending = false;
    
    qDebug() << "DEBUG: Stopped simulation, emitting simulationStopped";
    emit simulationStopped();
}

void CircuitSimulator::reset()
{
    qDebug() << "DEBUG: CircuitSimulator::reset() called";
    
    QMutexLocker locker(&m_simulationMutex);
    
    // Reset all components to initial state
    const QVector<Component*> &components = m_circuit->getComponents();
    qDebug() << "DEBUG: Resetting" << components.size() << "components";
    
    for (Component* comp : components) {
        comp->reset();
    }
    
    // Reset simulation state
    m_initialized = false;
    m_iterationCount = 0;
    m_simulationTime = 0.0;
    
    qDebug() << "DEBUG: Emitting simulationReset";
    emit simulationReset();
}

void CircuitSimulator::step()
{
    qDebug() << "DEBUG: CircuitSimulator::step() called";
    
    if (!m_running) {
        // Initialize if needed
        if (!m_initialized) {
            qDebug() << "DEBUG: Not initialized in step(), calling initialize()";
            if (!initialize()) {
                qDebug() << "DEBUG: Initialize failed in step()";
                return;
            }
        }
    }
    
    // Perform a single simulation step
    qDebug() << "DEBUG: Calling solve() from step()";
    solve();
}

void CircuitSimulator::triggerUpdate()
{
    qDebug() << "DEBUG: CircuitSimulator::triggerUpdate() called";
    qDebug() << "DEBUG: isUpdating:" << m_isUpdating << "updatePending:" << m_updatePending;
    
    // Prevent recursive simulation
    if (m_isUpdating) {
        // Already in an update cycle, do nothing
        qDebug() << "DEBUG: Ignoring recursive simulation update request";
        return;
    }
    
    // Throttle updates to avoid excessive calculations
    int elapsed = m_lastUpdateTime.elapsed();
    qDebug() << "DEBUG: Time since last update:" << elapsed << "ms, minimum interval:" << m_minUpdateIntervalMs << "ms";
    
    if (elapsed < m_minUpdateIntervalMs) {
        // If update is already pending, do nothing
        if (!m_updatePending) {
            int delay = m_minUpdateIntervalMs - elapsed;
            qDebug() << "DEBUG: Throttling simulation update, scheduled in" << delay << "ms";
            m_updateTimer.start(std::max(1, delay));
            m_updatePending = true;
        } else {
            qDebug() << "DEBUG: Update already pending, not scheduling another";
        }
        return;
    }
    
    // Start immediate update
    qDebug() << "DEBUG: Starting immediate update";
    doUpdate();
}

void CircuitSimulator::doUpdate()
{
    qDebug() << "DEBUG: CircuitSimulator::doUpdate() called";
    
    QMutexLocker locker(&m_simulationMutex);
    
    m_isUpdating = true;
    m_updatePending = false;
    m_lastUpdateTime.restart();
    
    qDebug() << "DEBUG: Set isUpdating=true, running=" << m_running;
    
    if (m_running) {
        // Solve the circuit equations
        qDebug() << "DEBUG: Calling solve() from doUpdate()";
        solve();
    } else {
        qDebug() << "DEBUG: Not running, skipping solve()";
    }
    
    m_isUpdating = false;
    qDebug() << "DEBUG: CircuitSimulator::doUpdate() completed, isUpdating=false";
}

// void CircuitSimulator::onComponentChanged()
// {
//     qDebug() << "DEBUG: CircuitSimulator::onComponentChanged() called";
    
//     // A component has changed state - trigger a simulation update
//     if (m_running) {
//         qDebug() << "DEBUG: Component state changed, triggering simulation update";

//         if(m_running)
//         {
//             triggerUpdate();
//         }
//     } else {
//         qDebug() << "DEBUG: Component changed but simulation not running";
//     }
// }

void CircuitSimulator::onCircuitChanged()
{
    qDebug() << "DEBUG: CircuitSimulator::onCircuitChanged() called";
    
    // Circuit topology has changed - reinitialize and trigger update
    qDebug() << "Circuit topology changed, reinitializing simulation";
    m_initialized = false;
    
    if (m_running) {
        qDebug() << "DEBUG: Circuit changed and running, triggering update";
        if(m_running)
        {
            triggerUpdate();
        }
    } else {
        qDebug() << "DEBUG: Circuit changed but not running";
    }
}

bool CircuitSimulator::buildMatrices()
{
    qDebug() << "DEBUG: CircuitSimulator::buildMatrices() starting";
    
    if (!m_circuit || !m_matrixSolver) {
        qDebug() << "DEBUG: No circuit or matrix solver";
        return false;
    }
    
    // Clear the matrix for a fresh build
    qDebug() << "DEBUG: Clearing matrix solver";
    m_matrixSolver->clear();
    
    // Process each electrical component
    const QVector<Component*> &components = m_circuit->getComponents();
    qDebug() << "DEBUG: Processing" << components.size() << "components for matrix building";
    
    int processedComponents = 0;
    
    for (Component* comp : components) {
        ElectricalComponent* elecComp = qobject_cast<ElectricalComponent*>(comp);
        if (!elecComp) {
            qDebug() << "DEBUG: Skipping non-electrical component:" << comp->getName();
            continue;
        }
        
        processedComponents++;
        qDebug() << "DEBUG: Processing electrical component" << processedComponents << ":" << elecComp->getName();
        
        // Get component resistance/conductance
        double resistance = elecComp->getResistance();
        qDebug() << "DEBUG: Component resistance:" << resistance << "ohms";
        
        if (resistance <= 0.0) {
            // For ideal voltage sources, use a very small resistance
            // to avoid division by zero
            resistance = 1e-6;
            qDebug() << "DEBUG: Zero/negative resistance, using" << resistance;
        }
        double conductance = 1.0 / resistance;
        qDebug() << "DEBUG: Component conductance:" << conductance << "siemens";
        
        // Process based on terminal count
        int terminalCount = elecComp->getTerminalCount();
        qDebug() << "DEBUG: Component has" << terminalCount << "terminals";
        
        if (terminalCount == 1) {
            // Single terminal components (e.g., Arduino pins)
            Node* node = elecComp->getNode(0);
            if (!node) {
                qDebug() << "DEBUG: Single terminal component has no connected node";
                continue;
            }
            
            int nodeIndex = m_nodeIndices.value(node, -1);
            qDebug() << "DEBUG: Single terminal connected to node" << node->getId() << "matrix index" << nodeIndex;
            
            if (nodeIndex < 0) {
                qDebug() << "DEBUG: Invalid node index for single terminal";
                continue;
            }
            
            // Check if this is an Arduino pin in output mode
            ArduinoPin* pin = qobject_cast<ArduinoPin*>(elecComp);
            if (pin && pin->isOutput()) {
                double outputVoltage = pin->getVoltage();
                qDebug() << "DEBUG: Arduino output pin" << pin->getPinNumber() 
                         << "output voltage:" << outputVoltage << "V";
                
                if (outputVoltage > 0.01) { // Threshold to avoid floating point issues
                    // Add as voltage source
                    qDebug() << "DEBUG: Adding voltage source for Arduino pin";
                    m_matrixSolver->addVoltageSource(nodeIndex, -1, outputVoltage);
                } else {
                    // Pin is OUTPUT but at 0V, still add conductance
                    qDebug() << "DEBUG: Adding conductance for 0V Arduino output pin";
                    m_matrixSolver->addConductance(nodeIndex, -1, conductance);
                }
            } else {
                // Add as conductance to ground for input pins or non-Arduino components
                qDebug() << "DEBUG: Adding conductance to ground for single terminal";
                m_matrixSolver->addConductance(nodeIndex, -1, conductance);
            }
        }
        else if (terminalCount == 2) {
            // Two terminal components (e.g., resistors, LEDs)
            Node* node1 = elecComp->getNode(0);
            Node* node2 = elecComp->getNode(1);
            
            if (!node1 || !node2) {
                qDebug() << "DEBUG: Two terminal component missing node connections";
                continue;
            }
            
            int nodeIndex1 = m_nodeIndices.value(node1, -1);
            int nodeIndex2 = m_nodeIndices.value(node2, -1);
            
            qDebug() << "DEBUG: Two terminal connected to nodes" << node1->getId() 
                     << "(index" << nodeIndex1 << ") and" << node2->getId() << "(index" << nodeIndex2 << ")";
            
            if (nodeIndex1 < 0 || nodeIndex2 < 0) {
                qDebug() << "DEBUG: Invalid node indices for two terminal component";
                continue;
            }
            
            // Add conductance between the two nodes
            qDebug() << "DEBUG: Adding conductance between nodes";
            m_matrixSolver->addConductance(nodeIndex1, nodeIndex2, conductance);
        }
        else {
            qDebug() << "DEBUG: Unsupported terminal count:" << terminalCount;
        }
    }
    
    qDebug() << "DEBUG: Processed" << processedComponents << "electrical components";
    qDebug() << "DEBUG: CircuitSimulator::buildMatrices() completed successfully";
    return true;
}

bool CircuitSimulator::updateComponentStates()
{
    qDebug() << "DEBUG: CircuitSimulator::updateComponentStates() starting";
    
    if (!m_circuit || !m_matrixSolver) {
        qDebug() << "DEBUG: No circuit or matrix solver for state update";
        return false;
    }
    
    // Process each electrical component
    const QVector<Component*> &components = m_circuit->getComponents();
    qDebug() << "DEBUG: Updating states for" << components.size() << "components";
    
    int updatedComponents = 0;
    
    for (Component* comp : components) {
        ElectricalComponent* elecComp = qobject_cast<ElectricalComponent*>(comp);
        if (!elecComp) continue;
        
        updatedComponents++;
        qDebug() << "DEBUG: Updating component" << updatedComponents << ":" << elecComp->getName();
        
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
            
            qDebug() << "DEBUG: Single terminal" << elecComp->getName() 
                     << "V:" << voltage << "I:" << current;
            
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
            double current = voltage / elecComp->getResistance();
            
            qDebug() << "DEBUG: Two terminal" << elecComp->getName() 
                     << "V1:" << v1 << "V2:" << v2 << "V_diff:" << voltage << "I:" << current;
            
            // Update component with new values but don't trigger recursive update
            elecComp->updateState(voltage, current);
        }
    }
    
    qDebug() << "DEBUG: Updated" << updatedComponents << "electrical components";
    qDebug() << "DEBUG: CircuitSimulator::updateComponentStates() completed successfully";
    return true;
}

bool CircuitSimulator::hasConverged()
{
    qDebug() << "DEBUG: CircuitSimulator::hasConverged() starting";
    
    if (m_prevValues.isEmpty()) {
        qDebug() << "DEBUG: No previous values, not converged";
        return false;
    }
    
    // Check each electrical component for change in voltage/current
    const QVector<Component*> &components = m_circuit->getComponents();
    qDebug() << "DEBUG: Checking convergence for" << components.size() << "components";
    
    int checkedComponents = 0;
    
    for (Component* comp : components) {
        ElectricalComponent* elecComp = qobject_cast<ElectricalComponent*>(comp);
        if (!elecComp) continue;
        
        checkedComponents++;
        
        // Get previous values
        auto prevIt = m_prevValues.find(elecComp);
        if (prevIt == m_prevValues.end()) {
            // No previous value - add current values and consider not converged
            m_prevValues[elecComp] = qMakePair(elecComp->getVoltage(), elecComp->getCurrent());
            qDebug() << "DEBUG: No previous value for" << elecComp->getName() << "- not converged";
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
        
        qDebug() << "DEBUG: Component" << elecComp->getName() 
                 << "V_diff:" << voltageDiff << "I_diff:" << currentDiff 
                 << "tolerance:" << m_convergenceTolerance;
        
        if (voltageDiff > m_convergenceTolerance || 
            currentDiff > m_convergenceTolerance) {
            
            qDebug() << "DEBUG: Component" << elecComp->getName() << "not converged";
            
            // Update previous values for next iteration
            m_prevValues[elecComp] = qMakePair(voltage, current);
            return false;
        }
        
        // Update previous values for next iteration even if converged
        m_prevValues[elecComp] = qMakePair(voltage, current);
    }
    
    qDebug() << "DEBUG: Checked" << checkedComponents << "components - ALL CONVERGED";
    return true;
}

void CircuitSimulator::assignNodeIds()
{
    qDebug() << "DEBUG: CircuitSimulator::assignNodeIds() starting";
    
    m_nodeIndices.clear();
    
    if (!m_circuit) {
        qDebug() << "DEBUG: No circuit for node assignment";
        return;
    }
    
    const QVector<Node*> &nodes = m_circuit->getNodes();
    qDebug() << "DEBUG: Found" << nodes.size() << "nodes in circuit";
    
    // Find ground node first
    Node* groundNode = m_circuit->getGroundNode();
    if (groundNode) {
        // Ground node is always index 0
        m_nodeIndices[groundNode] = 0;
        qDebug() << "DEBUG: Assigned ground node" << groundNode->getId() << "to matrix index 0";
    } else {
        qDebug() << "DEBUG: No ground node found";
    }
    
    // Assign indices to all other nodes
    int nextIndex = (groundNode) ? 1 : 0;
    for (Node* node : nodes) {
        if (node != groundNode) {
            m_nodeIndices[node] = nextIndex;
            qDebug() << "DEBUG: Assigned node" << node->getId() << "to matrix index" << nextIndex;
            nextIndex++;
        }
    }
    
    qDebug() << "DEBUG: Assigned indices to" << m_nodeIndices.size() << "nodes";
}

int CircuitSimulator::getNodeCount() const
{
    return m_nodeIndices.size();
}