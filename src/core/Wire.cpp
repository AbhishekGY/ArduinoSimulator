#include "core/Wire.h"
#include <QDebug>
#include <QtMath>
#include <algorithm>

Wire::Wire(QObject *parent)
    : ElectricalComponent("Wire", 2, parent)
    , m_length(0.0)
    , m_wireGauge(22)  // 22 AWG default (common for breadboard jumpers)
    , m_useCalculatedResistance(false) // Default to ideal wire
{
}

double Wire::getResistance() const
{
    if (m_useCalculatedResistance) {
        return calculateResistance();
    } else {
        // Ideal wire with very small but non-zero resistance
        return IDEAL_RESISTANCE;
    }
}

void Wire::updateState(double voltage, double current)
{
    // Wire passes voltage/current through with minimal drop
    ElectricalComponent::updateState(voltage, current);
    
    // For very small resistance, voltage drop is negligible
    // Current flows freely through ideal wire
}

void Wire::addPoint(const QPointF& point)
{
    m_points.append(point);
    calculateLength();
    emit componentChanged();
}

void Wire::setPoints(const QVector<QPointF>& points)
{
    m_points = points;
    calculateLength();
    emit componentChanged();
}

void Wire::clearPoints()
{
    m_points.clear();
    m_length = 0.0;
    emit componentChanged();
}

double Wire::getLength() const
{
    return m_length;
}

void Wire::calculateLength()
{
    m_length = 0.0;
    
    if (m_points.size() < 2) {
        return;
    }
    
    // Calculate total length as sum of segment lengths
    for (int i = 1; i < m_points.size(); ++i) {
        QPointF p1 = m_points[i - 1];
        QPointF p2 = m_points[i];
        
        double dx = p2.x() - p1.x();
        double dy = p2.y() - p1.y();
        double segmentLength = qSqrt(dx * dx + dy * dy);
        
        m_length += segmentLength;
    }
    
    // Convert from UI units (pixels) to physical units (meters)
    // Assume 1 pixel = 1mm for simulation purposes
    m_length *= 0.001; // Convert mm to meters
}

void Wire::setWireGauge(int gauge)
{
    if (gauge > 0 && gauge <= 50) { // Valid AWG range
        m_wireGauge = gauge;
        emit componentChanged();
        
        qDebug() << "Wire gauge set to" << gauge << "AWG";
    } else {
        qWarning() << "Invalid wire gauge:" << gauge;
    }
}

double Wire::calculateResistance() const
{
    if (m_length <= 0.0) {
        return IDEAL_RESISTANCE;
    }
    
    // Calculate wire diameter from AWG
    // AWG to diameter formula: d(mm) = 0.127 * 92^((36-AWG)/39)
    double diameterMm = 0.127 * qPow(92.0, (36.0 - m_wireGauge) / 39.0);
    double diameterM = diameterMm * 0.001; // Convert to meters
    double area = M_PI * (diameterM / 2.0) * (diameterM / 2.0); // Cross-sectional area
    
    // Calculate resistance: R = ρL/A
    double resistance = (COPPER_RESISTIVITY * m_length) / area;
    
    // Ensure minimum resistance for numerical stability
    return std::max(resistance, IDEAL_RESISTANCE);
}

// Factory method for creating common wire types
Wire* Wire::createJumperWire(const QPointF& start, const QPointF& end, QObject* parent)
{
    Wire* wire = new Wire(parent);
    wire->setName("Jumper Wire");
    wire->setWireGauge(22); // Standard jumper wire
    wire->addPoint(start);
    wire->addPoint(end);
    wire->setUseCalculatedResistance(false); // Ideal for jumpers
    
    return wire;
}

Wire* Wire::createBreadboardWire(const QVector<QPointF>& points, QObject* parent)
{
    Wire* wire = new Wire(parent);
    wire->setName("Breadboard Wire");
    wire->setWireGauge(22);
    wire->setPoints(points);
    wire->setUseCalculatedResistance(false); // Ideal for breadboard
    
    return wire;
}

Wire* Wire::createRealWire(const QVector<QPointF>& points, int gauge, QObject* parent)
{
    Wire* wire = new Wire(parent);
    wire->setName(QString("Wire (%1 AWG)").arg(gauge));
    wire->setWireGauge(gauge);
    wire->setPoints(points);
    wire->setUseCalculatedResistance(true); // Use real resistance
    
    return wire;
}