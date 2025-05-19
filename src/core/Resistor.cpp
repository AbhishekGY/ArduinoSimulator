#include "core/Resistor.h"

Resistor::Resistor(double resistance, QObject *parent)
    : ElectricalComponent("Resistor", 2, parent)
    , m_resistance(resistance)
{
}

void Resistor::setResistance(double resistance)
{
    if (resistance > 0.0) {
        m_resistance = resistance;
        emit componentChanged();
    }
}
