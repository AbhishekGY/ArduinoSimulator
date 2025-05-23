#include "simulation/MatrixSolver.h"
#include <QDebug>
#include <cmath>
#include <algorithm>

MatrixSolver::MatrixSolver(QObject *parent)
    : QObject(parent)
    , m_dimension(0)
    , m_isSetup(false)
    , m_epsilon(1e-10)
{
}

MatrixSolver::~MatrixSolver()
{
    clear();
}

void MatrixSolver::setDimension(int dimension)
{
    if (dimension < 1) {
        qWarning() << "Invalid matrix dimension:" << dimension;
        return;
    }

    m_dimension = dimension;
    setupMatrices();
}

void MatrixSolver::clear()
{
#ifdef HAVE_EIGEN3
    m_conductanceMatrix.setZero();
    m_rightHandSide.setZero();
    m_solution.setZero();
#else
    // Zero out matrices but keep allocated memory
    for (int i = 0; i < m_dimension; ++i) {
        std::fill(m_conductanceMatrix[i].begin(), m_conductanceMatrix[i].end(), 0.0);
        m_rightHandSide[i] = 0.0;
        m_solution[i] = 0.0;
    }
#endif
    m_branchCurrents.clear();
    m_isSetup = true;
}

void MatrixSolver::setupMatrices()
{
    clear(); // Make sure previous data is cleared

#ifdef HAVE_EIGEN3
    // Initialize Eigen matrices
    m_conductanceMatrix.resize(m_dimension, m_dimension);
    m_rightHandSide.resize(m_dimension);
    m_solution.resize(m_dimension);
    
    m_conductanceMatrix.setZero();
    m_rightHandSide.setZero();
    m_solution.setZero();
#else
    // Initialize QVector matrices
    m_conductanceMatrix.resize(m_dimension);
    m_rightHandSide.resize(m_dimension);
    m_solution.resize(m_dimension);
    
    for (int i = 0; i < m_dimension; ++i) {
        m_conductanceMatrix[i].resize(m_dimension);
        std::fill(m_conductanceMatrix[i].begin(), m_conductanceMatrix[i].end(), 0.0);
        m_rightHandSide[i] = 0.0;
        m_solution[i] = 0.0;
    }
#endif

    m_isSetup = true;
}

void MatrixSolver::addConductance(int nodeA, int nodeB, double conductance)
{
    if (!m_isSetup || conductance < m_epsilon) {
        return; // Skip extremely small conductance or uninitalized state
    }

    // Handle component connected to ground (nodeB = -1)
    if (nodeB == -1) {
        // Add diagonal element (self-conductance)
        if (nodeA >= 0 && nodeA < m_dimension) {
#ifdef HAVE_EIGEN3
            m_conductanceMatrix(nodeA, nodeA) += conductance;
#else
            m_conductanceMatrix[nodeA][nodeA] += conductance;
#endif
        }
    } 
    // Handle component between two nodes
    else if (nodeA >= 0 && nodeA < m_dimension && nodeB >= 0 && nodeB < m_dimension) {
        // Add to diagonal elements (self-conductance)
#ifdef HAVE_EIGEN3
        m_conductanceMatrix(nodeA, nodeA) += conductance;
        m_conductanceMatrix(nodeB, nodeB) += conductance;
        
        // Add off-diagonal elements (mutual conductance)
        m_conductanceMatrix(nodeA, nodeB) -= conductance;
        m_conductanceMatrix(nodeB, nodeA) -= conductance;
#else
        m_conductanceMatrix[nodeA][nodeA] += conductance;
        m_conductanceMatrix[nodeB][nodeB] += conductance;
        
        m_conductanceMatrix[nodeA][nodeB] -= conductance;
        m_conductanceMatrix[nodeB][nodeA] -= conductance;
#endif
    } else {
        qWarning() << "Invalid node indices in addConductance:" << nodeA << nodeB;
    }
}

void MatrixSolver::addCurrentSource(int nodeA, int nodeB, double current)
{
    if (!m_isSetup || std::abs(current) < m_epsilon) {
        return; // Skip tiny currents or uninitialized state
    }

    // Current source connected from nodeA to nodeB
    // Positive current flows from nodeA to nodeB
    
    // Handle current flowing into a node from outside (ground)
    if (nodeB == -1) {
        if (nodeA >= 0 && nodeA < m_dimension) {
#ifdef HAVE_EIGEN3
            m_rightHandSide(nodeA) -= current; // Current flowing into nodeA from ground
#else
            m_rightHandSide[nodeA] -= current;
#endif
        }
    }
    // Handle current flowing from ground to a node
    else if (nodeA == -1) {
        if (nodeB >= 0 && nodeB < m_dimension) {
#ifdef HAVE_EIGEN3
            m_rightHandSide(nodeB) += current; // Current flowing into nodeB from ground
#else
            m_rightHandSide[nodeB] += current;
#endif
        }
    }
    // Handle current between two nodes
    else if (nodeA >= 0 && nodeA < m_dimension && nodeB >= 0 && nodeB < m_dimension) {
#ifdef HAVE_EIGEN3
        m_rightHandSide(nodeA) -= current; // Current leaving nodeA
        m_rightHandSide(nodeB) += current; // Current entering nodeB
#else
        m_rightHandSide[nodeA] -= current;
        m_rightHandSide[nodeB] += current;
#endif
    } else {
        qWarning() << "Invalid node indices in addCurrentSource:" << nodeA << nodeB;
    }
    
    // Track the branch current for later retrieval
    m_branchCurrents[qMakePair(nodeA, nodeB)] = current;
}

void MatrixSolver::addVoltageSource(int nodeA, int nodeB, double voltage)
{
    if (!m_isSetup) {
        return;
    }
    
    // Handle voltage source from a node to ground
    if (nodeB == -1) {
        if (nodeA >= 0 && nodeA < m_dimension) {
            setNodeVoltage(nodeA, voltage);
        }
    }
    // Handle voltage source between two nodes
    // Note: This is a simplified approach that forces a voltage difference
    // A more complete implementation would use the Modified Nodal Analysis (MNA) 
    // to properly handle voltage sources with current as an additional variable
    else if (nodeA >= 0 && nodeA < m_dimension && nodeB >= 0 && nodeB < m_dimension) {
        // Method 1: Set nodeA to voltage and nodeB to 0
        // This is a simplification that works when the circuit has a clear ground
        setNodeVoltage(nodeA, voltage);
        setNodeVoltage(nodeB, 0);
        
        // Method 2 (alternative): Use very high conductance
        // This approximates an ideal voltage source with a very low resistance
        // double largeConst = 1e6; // Very large conductance
        // addConductance(nodeA, nodeB, largeConst);
        // addCurrentSource(nodeA, nodeB, largeConst * voltage);
    }
}

void MatrixSolver::setNodeVoltage(int node, double voltage)
{
    if (!m_isSetup || node < 0 || node >= m_dimension) {
        qWarning() << "Invalid node in setNodeVoltage:" << node;
        return;
    }
    
    // Clear row and set diagonal to 1 (identity)
#ifdef HAVE_EIGEN3
    m_conductanceMatrix.row(node).setZero();
    m_conductanceMatrix(node, node) = 1.0;
    
    // Set right-hand side to the known voltage
    m_rightHandSide(node) = voltage;
#else
    for (int i = 0; i < m_dimension; ++i) {
        m_conductanceMatrix[node][i] = 0.0;
    }
    m_conductanceMatrix[node][node] = 1.0;
    m_rightHandSide[node] = voltage;
#endif
}

bool MatrixSolver::solve()
{
    if (!m_isSetup || m_dimension == 0) {
        qWarning() << "Matrix not set up for solving.";
        return false;
    }
    
#ifdef HAVE_EIGEN3
    // Use Eigen's built-in solvers
    try {
        m_solution = m_conductanceMatrix.colPivHouseholderQr().solve(m_rightHandSide);
        return true;
    } catch (const std::exception& e) {
        qWarning() << "Matrix solver error:" << e.what();
        return false;
    }
#else
    // Use our own Gaussian elimination
    return gaussianElimination() && backSubstitution();
#endif
}

double MatrixSolver::getNodeVoltage(int node) const
{
    if (!m_isSetup || node < 0 || node >= m_dimension) {
        qWarning() << "Invalid node in getNodeVoltage:" << node;
        return 0.0;
    }
    
#ifdef HAVE_EIGEN3
    return m_solution(node);
#else
    return m_solution[node];
#endif
}

double MatrixSolver::getBranchCurrent(int nodeA, int nodeB) const
{
    // Check if we have a stored current source value first
    QPair<int, int> branchKey(nodeA, nodeB);
    if (m_branchCurrents.contains(branchKey)) {
        return m_branchCurrents[branchKey];
    }
    
    // If not a current source, calculate from node voltages and conductance
    // This assumes we've previously added the conductance for this branch
    if (nodeA >= 0 && nodeA < m_dimension && nodeB >= 0 && nodeB < m_dimension) {
#ifdef HAVE_EIGEN3
        double conductance = -m_conductanceMatrix(nodeA, nodeB);
        double voltageA = m_solution(nodeA);
        double voltageB = m_solution(nodeB);
#else
        double conductance = -m_conductanceMatrix[nodeA][nodeB];
        double voltageA = m_solution[nodeA];
        double voltageB = m_solution[nodeB];
#endif
        return conductance * (voltageA - voltageB);
    }
    
    // Handle connection to ground (nodeB = -1)
    if (nodeB == -1 && nodeA >= 0 && nodeA < m_dimension) {
#ifdef HAVE_EIGEN3
        double conductance = m_conductanceMatrix(nodeA, nodeA);
        double voltageA = m_solution(nodeA);
#else
        double conductance = m_conductanceMatrix[nodeA][nodeA];
        double voltageA = m_solution[nodeA];
#endif
        // Subtract other conductances to get just this branch
        for (int i = 0; i < m_dimension; ++i) {
            if (i != nodeA) {
#ifdef HAVE_EIGEN3
                conductance += m_conductanceMatrix(nodeA, i);
#else
                conductance += m_conductanceMatrix[nodeA][i];
#endif
            }
        }
        return conductance * voltageA;
    }
    
    return 0.0;
}

bool MatrixSolver::isValid() const
{
    if (!m_isSetup || m_dimension == 0) {
        return false;
    }
    
    // Check if the matrix is singular (determinant near zero)
#ifdef HAVE_EIGEN3
    // For smaller matrices, compute the determinant
    if (m_dimension <= 4) {
        double det = m_conductanceMatrix.determinant();
        return std::abs(det) > m_epsilon;
    }
    
    // For larger matrices, check if any diagonal element is too small
    // This is a simpler check that works with most Eigen versions
    for (int i = 0; i < m_dimension; ++i) {
        if (std::abs(m_conductanceMatrix(i, i)) < m_epsilon) {
            return false;
        }
    }
    
    // Additional check: see if matrix has reasonable values
    double maxElement = m_conductanceMatrix.cwiseAbs().maxCoeff();
    double minElement = m_conductanceMatrix.cwiseAbs().minCoeff();
    
    // Check for reasonable condition (max/min ratio)
    if (maxElement > 0 && minElement > 0) {
        double conditionEstimate = maxElement / minElement;
        return conditionEstimate < 1e12; // Reasonable condition number
    }
    
    return maxElement > m_epsilon;
#else
    // Simple check for obvious issues
    bool hasDiagonalElements = true;
    for (int i = 0; i < m_dimension; ++i) {
        if (std::abs(m_conductanceMatrix[i][i]) < m_epsilon) {
            hasDiagonalElements = false;
            break;
        }
    }
    return hasDiagonalElements;
#endif
}

#ifndef HAVE_EIGEN3
// Fallback implementations for when Eigen is not available

bool MatrixSolver::gaussianElimination()
{
    for (int i = 0; i < m_dimension; ++i) {
        // Find pivot element (largest in column)
        int pivotRow = findPivotRow(i);
        
        // If no suitable pivot found, matrix is singular
        if (std::abs(m_conductanceMatrix[pivotRow][i]) < m_epsilon) {
            qWarning() << "Singular matrix in gaussianElimination";
            return false;
        }
        
        // Swap rows if needed
        if (pivotRow != i) {
            swapRows(i, pivotRow);
        }
        
        // Normalize pivot row
        double pivot = m_conductanceMatrix[i][i];
        for (int j = i; j < m_dimension; ++j) {
            m_conductanceMatrix[i][j] /= pivot;
        }
        m_rightHandSide[i] /= pivot;
        
        // Eliminate below pivot
        for (int k = i + 1; k < m_dimension; ++k) {
            double factor = m_conductanceMatrix[k][i];
            for (int j = i; j < m_dimension; ++j) {
                m_conductanceMatrix[k][j] -= factor * m_conductanceMatrix[i][j];
            }
            m_rightHandSide[k] -= factor * m_rightHandSide[i];
        }
    }
    return true;
}

bool MatrixSolver::backSubstitution()
{
    for (int i = m_dimension - 1; i >= 0; --i) {
        m_solution[i] = m_rightHandSide[i];
        for (int j = i + 1; j < m_dimension; ++j) {
            m_solution[i] -= m_conductanceMatrix[i][j] * m_solution[j];
        }
    }
    return true;
}

void MatrixSolver::swapRows(int row1, int row2)
{
    if (row1 == row2) return;
    
    m_conductanceMatrix[row1].swap(m_conductanceMatrix[row2]);
    std::swap(m_rightHandSide[row1], m_rightHandSide[row2]);
}

int MatrixSolver::findPivotRow(int startRow)
{
    int pivotRow = startRow;
    double maxValue = std::abs(m_conductanceMatrix[startRow][startRow]);
    
    for (int i = startRow + 1; i < m_dimension; ++i) {
        double value = std::abs(m_conductanceMatrix[i][startRow]);
        if (value > maxValue) {
            maxValue = value;
            pivotRow = i;
        }
    }
    
    return pivotRow;
}
#endif // !HAVE_EIGEN3