#ifndef WIRE_H
#define WIRE_H

#include "ElectricalComponent.h"
#include <QPointF>
#include <QVector>

class Wire : public ElectricalComponent
{
    Q_OBJECT

public:
    explicit Wire(QObject *parent = nullptr);

    // Electrical properties
    double getResistance() const override;
    
    // Wire is essentially a zero-resistance connection between two nodes
    void updateState(double voltage, double current) override;

    // Geometric properties for UI representation
    const QVector<QPointF>& getPoints() const { return m_points; }
    void addPoint(const QPointF& point);
    void setPoints(const QVector<QPointF>& points);
    void clearPoints();
    
    // Wire properties
    double getLength() const;
    void setWireGauge(int gauge); // AWG gauge
    int getWireGauge() const { return m_wireGauge; }
    
    // Calculate actual resistance based on length and gauge
    double calculateResistance() const;
    void setUseCalculatedResistance(bool use) { m_useCalculatedResistance = use; }
    
    // Factory methods for common wire types
    static Wire* createJumperWire(const QPointF& start, const QPointF& end, QObject* parent = nullptr);
    static Wire* createBreadboardWire(const QVector<QPointF>& points, QObject* parent = nullptr);
    static Wire* createRealWire(const QVector<QPointF>& points, int gauge, QObject* parent = nullptr);

private:
    void calculateLength();

    // Physical properties
    QVector<QPointF> m_points;      // Wire path points for UI
    double m_length;                // Wire length in meters
    int m_wireGauge;               // AWG wire gauge (22 AWG default)
    bool m_useCalculatedResistance; // Whether to use calculated or ideal resistance
    
    // Electrical constants
    static constexpr double COPPER_RESISTIVITY = 1.68e-8; // Ohm⋅m at 20°C
    static constexpr double IDEAL_RESISTANCE = 1e-6;      // Very small for ideal wire
};

#endif // WIRE_H