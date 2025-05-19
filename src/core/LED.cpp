#include "core/LED.h"
#include <algorithm>
#include <cmath>
#include <QDebug>

LED::LED(const QColor &color, QObject *parent)
    : ElectricalComponent("LED", 2, parent)
    , m_isOn(false)
    , m_brightness(0.0)
    , m_color(color)
    , m_forwardVoltage(1.8)      // Typical red LED forward voltage
    , m_forwardCurrent(0.02)     // 20mA typical operating current
    , m_maxCurrent(0.025)        // 25mA maximum safe current
    , m_dynamicResistance(OFF_RESISTANCE)
    , m_isOverloaded(false)
    , m_thermalLimit(0.1)        // 100mW maximum power dissipation
{
    // Adjust forward voltage based on LED color
    if (color == Qt::blue || color == Qt::white) {
        m_forwardVoltage = 3.2;  // Blue/White LEDs have higher Vf
    } else if (color == Qt::green) {
        m_forwardVoltage = 2.2;  // Green LEDs
    } else if (color == Qt::yellow) {
        m_forwardVoltage = 2.0;  // Yellow LEDs
    }
    // Red LEDs default to 1.8V
}

double LED::getResistance() const
{
    return m_dynamicResistance;
}

void LED::updateState(double voltage, double current)
{
    // Store previous state for change detection
    bool wasOn = m_isOn;
    double prevBrightness = m_brightness;

    // Update base electrical properties
    ElectricalComponent::updateState(voltage, current);

    // Calculate new LED state
    calculateElectricalState();
    
    // Check for overload condition
    checkOverloadCondition();

    // Emit signals if state changed
    if (wasOn != m_isOn || std::abs(prevBrightness - m_brightness) > 0.01) {
        emit ledStateChanged(m_isOn, m_brightness);
        emit componentChanged();
    }
}

void LED::calculateElectricalState()
{
    // LED conduction model:
    // - Below forward voltage: essentially open circuit (very high resistance)
    // - Above forward voltage: exponential I-V characteristic
    
    double absCurrent = std::abs(m_current);
    double absVoltage = std::abs(m_voltage);
    
    // Determine if LED is conducting
    // LED only conducts in forward bias (positive voltage, positive current)
    bool forwardBiased = (m_voltage > 0 && m_current > 0);
    bool aboveThreshold = absVoltage >= m_forwardVoltage;
    bool sufficientCurrent = absCurrent > MIN_CONDUCTION_CURRENT;
    
    m_isOn = forwardBiased && aboveThreshold && sufficientCurrent;
    
    if (m_isOn) {
        // LED is conducting - calculate brightness and dynamic resistance
        m_brightness = calculateBrightness(absCurrent);
        m_dynamicResistance = calculateDynamicResistance(absCurrent);
    } else {
        // LED is off - high resistance, no light
        m_brightness = 0.0;
        m_dynamicResistance = OFF_RESISTANCE;
    }
}

double LED::calculateBrightness(double current) const
{
    // Brightness model: approximately linear with current up to rated current
    // Beyond rated current, brightness saturates
    
    if (current <= MIN_CONDUCTION_CURRENT) {
        return 0.0;
    }
    
    // Linear region up to forward current
    if (current <= m_forwardCurrent) {
        return current / m_forwardCurrent;
    }
    
    // Saturation region - logarithmic increase
    double excess = current - m_forwardCurrent;
    double maxExcess = m_maxCurrent - m_forwardCurrent;
    
    if (maxExcess > 0) {
        double saturationFactor = 1.0 + 0.3 * std::log(1.0 + excess / maxExcess);
        return std::min(1.0, saturationFactor);
    }
    
    return 1.0;
}

double LED::calculateDynamicResistance(double current) const
{
    // Diode equation: I = Is * (e^(V/nVt) - 1)
    // Dynamic resistance: rd = nVt / I
    // For simplicity, we use an empirical model
    
    if (current <= MIN_CONDUCTION_CURRENT) {
        return OFF_RESISTANCE;
    }
    
    // Empirical model: rd = Vf / I + rs
    // where rs is series resistance (~10-50 ohms for typical LEDs)
    double seriesResistance = 25.0; // Ohms
    double dynamicComponent = m_forwardVoltage / current;
    
    return seriesResistance + dynamicComponent;
}

void LED::checkOverloadCondition()
{
    double power = getPowerDissipation();
    double absCurrent = std::abs(m_current);
    
    bool wasOverloaded = m_isOverloaded;
    
    // Check current limit
    if (absCurrent > m_maxCurrent) {
        m_isOverloaded = true;
        qWarning() << "LED current overload:" << absCurrent * 1000 << "mA (max:" << m_maxCurrent * 1000 << "mA)";
    }
    // Check thermal limit
    else if (power > m_thermalLimit) {
        m_isOverloaded = true;
        qWarning() << "LED thermal overload:" << power * 1000 << "mW (max:" << m_thermalLimit * 1000 << "mW)";
    }
    else {
        m_isOverloaded = false;
    }
    
    // Emit signal on new overload condition
    if (!wasOverloaded && m_isOverloaded) {
        emit overloadDetected();
    }
}

double LED::getPowerDissipation() const
{
    return std::abs(m_voltage * m_current);
}

void LED::setColor(const QColor &color)
{
    if (m_color != color) {
        m_color = color;
        
        // Adjust forward voltage for new color
        if (color == Qt::blue || color == Qt::white) {
            setForwardVoltage(3.2);
        } else if (color == Qt::green) {
            setForwardVoltage(2.2);
        } else if (color == Qt::yellow) {
            setForwardVoltage(2.0);
        } else { // Red or other colors
            setForwardVoltage(1.8);
        }
        
        emit componentChanged();
    }
}

void LED::setForwardVoltage(double voltage)
{
    if (voltage > 0.0 && voltage != m_forwardVoltage) {
        m_forwardVoltage = voltage;
        
        // Recalculate state if circuit is active
        if (getCircuit()) {
            calculateElectricalState();
        }
        
        emit componentChanged();
    }
}

void LED::setMaxCurrent(double current)
{
    if (current > 0.0 && current != m_maxCurrent) {
        m_maxCurrent = current;
        checkOverloadCondition();
        emit componentChanged();
    }
}

void LED::reset()
{
    ElectricalComponent::reset();
    
    m_isOn = false;
    m_brightness = 0.0;
    m_dynamicResistance = OFF_RESISTANCE;
    m_isOverloaded = false;
    
    emit ledStateChanged(m_isOn, m_brightness);
}

LED* LED::createStandardLED(const QString &type, QObject *parent)
{
    LED *led = nullptr;
    
    if (type.toLower() == "red") {
        led = new LED(Qt::red, parent);
        led->setForwardVoltage(1.8);
        led->setMaxCurrent(0.025);
    }
    else if (type.toLower() == "green") {
        led = new LED(Qt::green, parent);
        led->setForwardVoltage(2.2);
        led->setMaxCurrent(0.025);
    }
    else if (type.toLower() == "blue") {
        led = new LED(Qt::blue, parent);
        led->setForwardVoltage(3.2);
        led->setMaxCurrent(0.020);
    }
    else if (type.toLower() == "white") {
        led = new LED(Qt::white, parent);
        led->setForwardVoltage(3.2);
        led->setMaxCurrent(0.020);
    }
    else if (type.toLower() == "yellow") {
        led = new LED(Qt::yellow, parent);
        led->setForwardVoltage(2.0);
        led->setMaxCurrent(0.025);
    }
    else {
        // Default to red LED
        led = new LED(Qt::red, parent);
        qWarning() << "Unknown LED type:" << type << "- defaulting to red";
    }
    
    led->setName(type + " LED");
    return led;
}