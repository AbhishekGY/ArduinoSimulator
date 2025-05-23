#ifndef MATRIXSOLVER_H
#define MATRIXSOLVER_H

#include <QObject>
#include <QVector>
#include <QHash>

// If Eigen is available, we'll use it for better performance
#ifdef HAVE_EIGEN3
#include <Eigen/Dense>
#endif

class MatrixSolver : public QObject
{
    Q_OBJECT

public:
    explicit MatrixSolver(QObject *parent = nullptr);
    ~MatrixSolver();

    // Set matrix dimensions (nodes count)
    void setDimension(int dimension);

    // Clear all values and reset the solver
    void clear();
    
    // Add component conductance between two nodes
    // (if nodeB is -1, it connects to ground)
    void addConductance(int nodeA, int nodeB, double conductance);
    
    // Add current source between two nodes
    // (positive current flows from nodeA to nodeB)
    void addCurrentSource(int nodeA, int nodeB, double current);
    
    // Set voltage source between two nodes
    // (nodeB is typically ground (-1) for Arduino pin sources)
    void addVoltageSource(int nodeA, int nodeB, double voltage);
    
    // Set a node to a known voltage (like ground = 0V)
    void setNodeVoltage(int node, double voltage);
    
    // Solve the system of equations
    bool solve();
    
    // Get results after solving
    double getNodeVoltage(int node) const;
    double getBranchCurrent(int nodeA, int nodeB) const;
    
    // Get the number of nodes
    int getDimension() const { return m_dimension; }
    
    // Check if the matrix is consistent and well-conditioned
    bool isValid() const;

private:
    void setupMatrices();

#ifdef HAVE_EIGEN3
    // Implementation using Eigen (preferred)
    Eigen::MatrixXd m_conductanceMatrix;  // The 'A' matrix (conductance)
    Eigen::VectorXd m_rightHandSide;      // The 'b' vector (current sources)
    Eigen::VectorXd m_solution;           // The solved node voltages
#else
    // Fallback implementation using QVector
    QVector<QVector<double>> m_conductanceMatrix;
    QVector<double> m_rightHandSide;
    QVector<double> m_solution;
    
    // Helper methods for the fallback implementation
    bool gaussianElimination();
    bool backSubstitution();
    void swapRows(int row1, int row2);
    int findPivotRow(int startRow);
#endif

    // Common attributes
    int m_dimension;           // Number of nodes in the circuit
    bool m_isSetup;            // Whether matrices are initialized
    double m_epsilon;          // Small value for numerical stability
    
    // Mapping to track branch currents
    struct BranchKey {
        int nodeA;
        int nodeB;
        bool operator==(const BranchKey& other) const {
            return (nodeA == other.nodeA && nodeB == other.nodeB) ||
                   (nodeA == other.nodeB && nodeB == other.nodeA);
        }
    };
    QHash<QPair<int, int>, double> m_branchCurrents;
};

#endif // MATRIXSOLVER_H