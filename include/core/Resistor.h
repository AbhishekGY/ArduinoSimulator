#ifndef RESISTOR_H
#define RESISTOR_H

#include "ElectricalComponent.h"

class Resistor : public ElectricalComponent
{
    Q_OBJECT

public:
    explicit Resistor(double resistance = 1000.0, QObject *parent = nullptr);

    // Electrical properties
    double getResistance() const override { return m_resistance; }
    void setResistance(double resistance);

    // Power calculation
    double getPower() const { return m_voltage * m_current; }

private:
    double m_resistance; // Ohms
};

#endif // RESISTOR_H
