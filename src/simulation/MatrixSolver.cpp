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
    qDebug() << "DEBUG: MatrixSolver constructor";
}

MatrixSolver::~MatrixSolver()
{
    qDebug() << "DEBUG: MatrixSolver destructor";
    clear();
}

void MatrixSolver::setDimension(int dimension)
{
    qDebug() << "DEBUG: MatrixSolver::setDimension(" << dimension << ")";
    
    if (dimension < 1) {
        qWarning() << "Invalid matrix dimension:" << dimension;
        return;
    }

    m_dimension = dimension;
    qDebug() << "DEBUG: Setting up matrices for dimension" << dimension;
    setupMatrices();
}

void MatrixSolver::clear()
{
    qDebug() << "DEBUG: MatrixSolver::clear()";
    
#ifdef HAVE_EIGEN3
    if (m_dimension > 0) {
        m_conductanceMatrix.setZero();
        m_rightHandSide.setZero();
        m_solution.setZero();
        qDebug() << "DEBUG: Cleared Eigen matrices";
    }
#else
    // Zero out matrices but keep allocated memory
    for (int i = 0; i < m_dimension; ++i) {
        std::fill(m_conductanceMatrix[i].begin(), m_conductanceMatrix[i].end(), 0.0);
        m_rightHandSide[i] = 0.0;
        m_solution[i] = 0.0;
    }
    qDebug() << "DEBUG: Cleared QVector matrices";
#endif
    m_branchCurrents.clear();
    m_isSetup = true;
    qDebug() << "DEBUG: MatrixSolver::clear() completed";
}

void MatrixSolver::setupMatrices()
{
    qDebug() << "DEBUG: MatrixSolver::setupMatrices() for dimension" << m_dimension;
    
    clear(); // Make sure previous data is cleared

#ifdef HAVE_EIGEN3
    qDebug() << "DEBUG: Using Eigen3 implementation";
    // Initialize Eigen matrices
    m_conductanceMatrix.resize(m_dimension, m_dimension);
    m_rightHandSide.resize(m_dimension);
    m_solution.resize(m_dimension);
    
    m_conductanceMatrix.setZero();
    m_rightHandSide.setZero();
    m_solution.setZero();
    qDebug() << "DEBUG: Eigen matrices initialized";
#else
    qDebug() << "DEBUG: Using QVector fallback implementation";
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
    qDebug() << "DEBUG: QVector matrices initialized";
#endif

    m_isSetup = true;
    qDebug() << "DEBUG: MatrixSolver::setupMatrices() completed";
}

void MatrixSolver::addConductance(int nodeA, int nodeB, double conductance)
{
    qDebug() << "DEBUG: MatrixSolver::addConductance(" << nodeA << "," << nodeB << "," << conductance << ")";
    
    if (!m_isSetup || conductance < m_epsilon) {
        qDebug() << "DEBUG: Skipping conductance - not setup:" << !m_isSetup << "too small:" << (conductance < m_epsilon);
        return; // Skip extremely small conductance or uninitalized state
    }

    // Handle component connected to ground (nodeB = -1)
    if (nodeB == -1) {
        qDebug() << "DEBUG: Adding conductance to ground for node" << nodeA;
        // Add diagonal element (self-conductance)
        if (nodeA >= 0 && nodeA < m_dimension) {
#ifdef HAVE_EIGEN3
            m_conductanceMatrix(nodeA, nodeA) += conductance;
            qDebug() << "DEBUG: Added" << conductance << "to diagonal element (" << nodeA << "," << nodeA << ")";
#else
            m_conductanceMatrix[nodeA][nodeA] += conductance;
            qDebug() << "DEBUG: Added" << conductance << "to diagonal element [" << nodeA << "][" << nodeA << "]";
#endif
        } else {
            qDebug() << "DEBUG: Invalid nodeA index:" << nodeA;
        }
    } 
    // Handle component between two nodes
    else if (nodeA >= 0 && nodeA < m_dimension && nodeB >= 0 && nodeB < m_dimension) {
        qDebug() << "DEBUG: Adding conductance between nodes" << nodeA << "and" << nodeB;
        // Add to diagonal elements (self-conductance)
#ifdef HAVE_EIGEN3
        m_conductanceMatrix(nodeA, nodeA) += conductance;
        m_conductanceMatrix(nodeB, nodeB) += conductance;
        
        // Add off-diagonal elements (mutual conductance)
        m_conductanceMatrix(nodeA, nodeB) -= conductance;
        m_conductanceMatrix(nodeB, nodeA) -= conductance;
        qDebug() << "DEBUG: Added/subtracted" << conductance << "to matrix elements (Eigen)";
#else
        m_conductanceMatrix[nodeA][nodeA] += conductance;
        m_conductanceMatrix[nodeB][nodeB] += conductance;
        
        m_conductanceMatrix[nodeA][nodeB] -= conductance;
        m_conductanceMatrix[nodeB][nodeA] -= conductance;
        qDebug() << "DEBUG: Added/subtracted" << conductance << "to matrix elements (QVector)";
#endif
    } else {
        qWarning() << "Invalid node indices in addConductance:" << nodeA << nodeB;
    }
}

void MatrixSolver::addCurrentSource(int nodeA, int nodeB, double current)
{
    qDebug() << "DEBUG: MatrixSolver::addCurrentSource(" << nodeA << "," << nodeB << "," << current << ")";
    
    if (!m_isSetup || std::abs(current) < m_epsilon) {
        qDebug() << "DEBUG: Skipping current source - not setup or too small";
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
            qDebug() << "DEBUG: Added current source from ground to node" << nodeA;
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
            qDebug() << "DEBUG: Added current source from ground to node" << nodeB;
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
        qDebug() << "DEBUG: Added current source between nodes" << nodeA << "and" << nodeB;
    } else {
        qWarning() << "Invalid node indices in addCurrentSource:" << nodeA << nodeB;
    }
    
    // Track the branch current for later retrieval
    m_branchCurrents[qMakePair(nodeA, nodeB)] = current;
}

void MatrixSolver::addVoltageSource(int nodeA, int nodeB, double voltage)
{
    qDebug() << "DEBUG: MatrixSolver::addVoltageSource(" << nodeA << "," << nodeB << "," << voltage << ")";
    
    if (!m_isSetup) {
        qDebug() << "DEBUG: Not setup, skipping voltage source";
        return;
    }
    
    // Handle voltage source from a node to ground
    if (nodeB == -1) {
        if (nodeA >= 0 && nodeA < m_dimension) {
            setNodeVoltage(nodeA, voltage);
            qDebug() << "DEBUG: Set voltage source from node" << nodeA << "to ground";
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
        qDebug() << "DEBUG: Set voltage source between nodes" << nodeA << "and" << nodeB;
    }
}

void MatrixSolver::setNodeVoltage(int node, double voltage)
{
    qDebug() << "DEBUG: MatrixSolver::setNodeVoltage(" << node << "," << voltage << ")";
    
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
    qDebug() << "DEBUG: Set node" << node << "voltage constraint (Eigen)";
#else
    for (int i = 0; i < m_dimension; ++i) {
        m_conductanceMatrix[node][i] = 0.0;
    }
    m_conductanceMatrix[node][node] = 1.0;
    m_rightHandSide[node] = voltage;
    qDebug() << "DEBUG: Set node" << node << "voltage constraint (QVector)";
#endif
}

bool MatrixSolver::solve()
{
    qDebug() << "DEBUG: MatrixSolver::solve() starting";
    
    if (!m_isSetup || m_dimension == 0) {
        qWarning() << "Matrix not set up for solving.";
        return false;
    }
    
    qDebug() << "DEBUG: Matrix dimension:" << m_dimension;
    
    // Print matrix for debugging (only for small matrices)
    if (m_dimension <= 5) {
        qDebug() << "DEBUG: Conductance Matrix:";
#ifdef HAVE_EIGEN3
        for (int i = 0; i < m_dimension; i++) {
            QString row;
            for (int j = 0; j < m_dimension; j++) {
                row += QString("%1 ").arg(m_conductanceMatrix(i, j), 8, 'f', 3);
            }
            qDebug() << "  " << row;
        }
        qDebug() << "DEBUG: Right Hand Side:";
        for (int i = 0; i < m_dimension; i++) {
            qDebug() << "  " << m_rightHandSide(i);
        }
#else
        for (int i = 0; i < m_dimension; i++) {
            QString row;
            for (int j = 0; j < m_dimension; j++) {
                row += QString("%1 ").arg(m_conductanceMatrix[i][j], 8, 'f', 3);
            }
            qDebug() << "  " << row;
        }
        qDebug() << "DEBUG: Right Hand Side:";
        for (int i = 0; i < m_dimension; i++) {
            qDebug() << "  " << m_rightHandSide[i];
        }
#endif
    }
    
#ifdef HAVE_EIGEN3
    qDebug() << "DEBUG: Using Eigen solver";
    // Use Eigen's built-in solvers
    try {
        qDebug() << "DEBUG: Starting Eigen QR decomposition";
        m_solution = m_conductanceMatrix.colPivHouseholderQr().solve(m_rightHandSide);
        qDebug() << "DEBUG: Eigen solve completed successfully";
        
        // Print solution for debugging
        if (m_dimension <= 5) {
            qDebug() << "DEBUG: Solution:";
            for (int i = 0; i < m_dimension; i++) {
                qDebug() << "  Node" << i << "=" << m_solution(i) << "V";
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        qWarning() << "Matrix solver error:" << e.what();
        return false;
    }
#else
    qDebug() << "DEBUG: Using fallback Gaussian elimination";
    // Use our own Gaussian elimination
    bool gaussResult = gaussianElimination();
    qDebug() << "DEBUG: Gaussian elimination result:" << gaussResult;
    
    if (!gaussResult) {
        return false;
    }
    
    bool backSubResult = backSubstitution();
    qDebug() << "DEBUG: Back substitution result:" << backSubResult;
    
    // Print solution for debugging
    if (backSubResult && m_dimension <= 5) {
        qDebug() << "DEBUG: Solution:";
        for (int i = 0; i < m_dimension; i++) {
            qDebug() << "  Node" << i << "=" << m_solution[i] << "V";
        }
    }
    
    return backSubResult;
#endif
}

double MatrixSolver::getNodeVoltage(int node) const
{
    if (!m_isSetup || node < 0 || node >= m_dimension) {
        qWarning() << "Invalid node in getNodeVoltage:" << node;
        return 0.0;
    }
    
#ifdef HAVE_EIGEN3
    double voltage = m_solution(node);
#else
    double voltage = m_solution[node];
#endif
    
    qDebug() << "DEBUG: getNodeVoltage(" << node << ") =" << voltage;
    return voltage;
}

double MatrixSolver::getBranchCurrent(int nodeA, int nodeB) const
{
    qDebug() << "DEBUG: MatrixSolver::getBranchCurrent(" << nodeA << "," << nodeB << ")";
    
    // Check if we have a stored current source value first
    QPair<int, int> branchKey(nodeA, nodeB);
    if (m_branchCurrents.contains(branchKey)) {
        double current = m_branchCurrents[branchKey];
        qDebug() << "DEBUG: Found stored current:" << current;
        return current;
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
        double current = conductance * (voltageA - voltageB);
        qDebug() << "DEBUG: Calculated current from voltages:" << current;
        return current;
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
        double current = conductance * voltageA;
        qDebug() << "DEBUG: Calculated current to ground:" << current;
        return current;
    }
    
    qDebug() << "DEBUG: No current found, returning 0";
    return 0.0;
}

bool MatrixSolver::isValid() const
{
    qDebug() << "DEBUG: MatrixSolver::isValid() checking";
    
    if (!m_isSetup || m_dimension == 0) {
        qDebug() << "DEBUG: Not valid - not setup or zero dimension";
        return false;
    }
    
    // Check if the matrix is singular (determinant near zero)
#ifdef HAVE_EIGEN3
    // For smaller matrices, compute the determinant
    if (m_dimension <= 4) {
        double det = m_conductanceMatrix.determinant();
        qDebug() << "DEBUG: Matrix determinant:" << det;
        bool valid = std::abs(det) > m_epsilon;
        qDebug() << "DEBUG: Determinant check result:" << valid;
        return valid;
    }
    
    // For larger matrices, check if any diagonal element is too small
    // This is a simpler check that works with most Eigen versions
    for (int i = 0; i < m_dimension; ++i) {
        if (std::abs(m_conductanceMatrix(i, i)) < m_epsilon) {
            qDebug() << "DEBUG: Small diagonal element at" << i << ":" << m_conductanceMatrix(i, i);
            return false;
        }
    }
    
    // Additional check: see if matrix has reasonable values
    double maxElement = m_conductanceMatrix.cwiseAbs().maxCoeff();
    double minElement = m_conductanceMatrix.cwiseAbs().minCoeff();
    qDebug() << "DEBUG: Matrix range - max:" << maxElement << "min:" << minElement;
    
    // Check for reasonable condition (max/min ratio)
    if (maxElement > 0 && minElement > 0) {
        double conditionEstimate = maxElement / minElement;
        qDebug() << "DEBUG: Condition estimate:" << conditionEstimate;
        return conditionEstimate < 1e12; // Reasonable condition number
    }
    
    bool valid = maxElement > m_epsilon;
    qDebug() << "DEBUG: Max element check result:" << valid;
    return valid;
#else
    qDebug() << "DEBUG: Using simple diagonal check for QVector implementation";
    // Simple check for obvious issues
    bool hasDiagonalElements = true;
    for (int i = 0; i < m_dimension; ++i) {
        if (std::abs(m_conductanceMatrix[i][i]) < m_epsilon) {
            qDebug() << "DEBUG: Small diagonal element at" << i << ":" << m_conductanceMatrix[i][i];
            hasDiagonalElements = false;
            break;
        }
    }
    qDebug() << "DEBUG: Diagonal elements check result:" << hasDiagonalElements;
    return hasDiagonalElements;
#endif
}

#ifndef HAVE_EIGEN3
// Fallback implementations for when Eigen is not available

bool MatrixSolver::gaussianElimination()
{
    qDebug() << "DEBUG: MatrixSolver::gaussianElimination() starting";
    
    for (int i = 0; i < m_dimension; ++i) {
        qDebug() << "DEBUG: Processing row" << i;
        
        // Find pivot element (largest in column)
        int pivotRow = findPivotRow(i);
        qDebug() << "DEBUG: Pivot row for column" << i << "is" << pivotRow;
        
        // If no suitable pivot found, matrix is singular
        if (std::abs(m_conductanceMatrix[pivotRow][i]) < m_epsilon) {
            qWarning() << "Singular matrix in gaussianElimination at row" << i;
            return false;
        }
        
        // Swap rows if needed
        if (pivotRow != i) {
            qDebug() << "DEBUG: Swapping rows" << i << "and" << pivotRow;
            swapRows(i, pivotRow);
        }
        
        // Normalize pivot row
        double pivot = m_conductanceMatrix[i][i];
        qDebug() << "DEBUG: Pivot element:" << pivot;
        
        for (int j = i; j < m_dimension; ++j) {
            m_conductanceMatrix[i][j] /= pivot;
        }
        m_rightHandSide[i] /= pivot;
        
        // Eliminate below pivot
        for (int k = i + 1; k < m_dimension; ++k) {
            double factor = m_conductanceMatrix[k][i];
            if (std::abs(factor) > m_epsilon) {
                qDebug() << "DEBUG: Eliminating row" << k << "with factor" << factor;
                for (int j = i; j < m_dimension; ++j) {
                    m_conductanceMatrix[k][j] -= factor * m_conductanceMatrix[i][j];
                }
                m_rightHandSide[k] -= factor * m_rightHandSide[i];
            }
        }
    }
    
    qDebug() << "DEBUG: Gaussian elimination completed successfully";
    return true;
}

bool MatrixSolver::backSubstitution()
{
    qDebug() << "DEBUG: MatrixSolver::backSubstitution() starting";
    
    for (int i = m_dimension - 1; i >= 0; --i) {
        qDebug() << "DEBUG: Back substitution for variable" << i;
        
        m_solution[i] = m_rightHandSide[i];
        for (int j = i + 1; j < m_dimension; ++j) {
            m_solution[i] -= m_conductanceMatrix[i][j] * m_solution[j];
        }
        
        qDebug() << "DEBUG: Solution[" << i << "] =" << m_solution[i];
    }
    
    qDebug() << "DEBUG: Back substitution completed successfully";
    return true;
}

void MatrixSolver::swapRows(int row1, int row2)
{
    if (row1 == row2) return;
    
    qDebug() << "DEBUG: Swapping rows" << row1 << "and" << row2;
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
    
    qDebug() << "DEBUG: Found pivot row" << pivotRow << "with value" << maxValue;
    return pivotRow;
}
#endif // !HAVE_EIGEN3