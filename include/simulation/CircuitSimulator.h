#ifndef CIRCUITSIMULATOR_H
#define CIRCUITSIMULATOR_H

#include <QObject>
#include <QVector>
#include <QMap>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QElapsedTimer>
#include <QMutex>

class Circuit;
class Component;
class ElectricalComponent;
class Node;
class MatrixSolver;

class CircuitSimulator : public QObject
{
    Q_OBJECT

public:
    explicit CircuitSimulator(Circuit *circuit, QObject *parent = nullptr);
    ~CircuitSimulator();

    // Simulation control
    bool initialize();
    bool solve();
    bool isRunning() const { return m_running; }

    // Simulation settings
    void setMaxIterations(int iterations) { m_maxIterations = iterations; }
    int getMaxIterations() const { return m_maxIterations; }
    
    void setConvergenceTolerance(double tolerance) { m_convergenceTolerance = tolerance; }
    double getConvergenceTolerance() const { return m_convergenceTolerance; }
    
    void setTimeStep(double timeStep) { m_timeStep = timeStep; }
    double getTimeStep() const { return m_timeStep; }
    
    // Minimum update interval to prevent excessive updates
    void setMinUpdateInterval(int msecs) { m_minUpdateIntervalMs = msecs; }
    int getMinUpdateInterval() const { return m_minUpdateIntervalMs; }
    
    // Simulation stats
    int getIterationCount() const { return m_iterationCount; }
    double getSimulationTime() const { return m_simulationTime; }
    
    // Circuit access
    Circuit* getCircuit() const { return m_circuit; }

public slots:
    void start();
    void stop();
    void reset();
    void step();
    
    // Trigger update - handles recursion prevention
    void triggerUpdate();
    
    // Handle component changes
    void onCircuitChanged();

signals:
    void simulationStarted();
    void simulationStopped();
    void simulationReset();
    void simulationStepCompleted(int step, double time);
    void simulationError(const QString &error);
    void convergenceAchieved();
    void convergenceFailed(int iterations);

private slots:
    // Actual update function called after debouncing
    void doUpdate();

private:
    // Matrix building and solving
    bool buildMatrices();
    bool updateComponentStates();
    bool hasConverged();
    
    // Utility functions
    void assignNodeIds();
    int getNodeCount() const;
    
    // Current circuit state
    Circuit *m_circuit;
    MatrixSolver *m_matrixSolver;
    
    // Node mapping (Node object to matrix index)
    QHash<Node*, int> m_nodeIndices;
    
    // Previous voltage/current values for convergence check
    QHash<ElectricalComponent*, QPair<double, double>> m_prevValues;
    
    // Simulation parameters
    int m_maxIterations;
    double m_convergenceTolerance;
    double m_timeStep;
    int m_minUpdateIntervalMs;
    
    // Simulation state
    bool m_running;
    bool m_initialized;
    int m_iterationCount;
    double m_simulationTime;
    
    // Protection against recursive updates
    bool m_isUpdating;
    bool m_updatePending;
    QElapsedTimer m_lastUpdateTime;
    QTimer m_updateTimer;
    
    // Thread safety
    QMutex m_simulationMutex;
};

#endif // CIRCUITSIMULATOR_H