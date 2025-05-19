#include "core/ArduinoPin.h"
#include "core/Arduino.h"
#include <QDebug>
#include <algorithm>
#include <cmath>

// ArduinoPin base class implementation
ArduinoPin::ArduinoPin(int pinNumber, PinType type, Arduino *arduino, QObject *parent)
    : ElectricalComponent(QString("Pin %1").arg(pinNumber), 1, parent)
    , m_pinNumber(pinNumber)
    , m_pinType(type)
    , m_mode(INPUT)
    , m_arduino(arduino)
    , m_outputVoltage(0.0)
    , m_inputVoltage(0.0)
    , m_outputCurrent(0.0)
    , m_setValue(0.0)
    , m_maxCurrent(0.04)        // 40mA typical limit
    , m_outputResistance(25.0)  // ~25 ohm output resistance
    , m_inputResistance(1e9)    // Very high input impedance
    , m_isOverloaded(false)
{
}

void ArduinoPin::setMode(PinMode mode)
{
    if (m_mode != mode) {
        PinMode oldMode = m_mode;
        m_mode = mode;
        
        // Reset pin state when mode changes
        if (oldMode == OUTPUT || oldMode == ANALOG_OUTPUT) {
            m_outputVoltage = 0.0;
            m_outputCurrent = 0.0;
        }
        
        emit pinModeChanged(mode);
        emit componentChanged();
        
        qDebug() << "Pin" << m_pinNumber << "mode changed to" << mode;
    }
}

double ArduinoPin::getResistance() const
{
    switch (m_mode) {
        case OUTPUT:
        case ANALOG_OUTPUT:
            return m_outputResistance;
            
        case INPUT:
            return m_inputResistance;
            
        case INPUT_PULLUP:
            return 50000.0; // 50kΩ pullup resistor
            
        case ANALOG_INPUT:
            return m_inputResistance;
            
        default:
            return m_inputResistance;
    }
}

void ArduinoPin::updateState(double voltage, double current)
{
    // Store previous values for change detection
    double prevVoltage = m_voltage;
    double prevCurrent = m_current;
    
    // Update base electrical state
    ElectricalComponent::updateState(voltage, current);
    
    if (isOutput()) {
        // For output pins, the voltage at the pin depends on the load
        // The pin tries to output m_outputVoltage but actual voltage depends on circuit
        m_outputCurrent = current;
        updateOutputState();
    } else {
        // For input pins, read the voltage applied to the pin
        m_inputVoltage = voltage;
        updateInputState();
    }
    
    // Check for overload conditions
    checkOverloadCondition();
    
    // Emit change signal if significant change occurred
    if (std::abs(voltage - prevVoltage) > 0.01 || std::abs(current - prevCurrent) > 0.001) {
        emit pinValueChanged(isOutput() ? m_outputVoltage : m_inputVoltage);
    }
}

void ArduinoPin::checkOverloadCondition()
{
    bool wasOverloaded = m_isOverloaded;
    double absCurrent = std::abs(m_outputCurrent);
    
    if (isOutput() && absCurrent > m_maxCurrent) {
        m_isOverloaded = true;
        qWarning() << "Pin" << m_pinNumber << "current overload:" 
                   << absCurrent * 1000 << "mA (max:" << m_maxCurrent * 1000 << "mA)";
    } else {
        m_isOverloaded = false;
    }
    
    if (!wasOverloaded && m_isOverloaded) {
        emit pinOverloaded();
    }
}

void ArduinoPin::updateOutputState()
{
    // Base implementation - derived classes override this
    // The actual pin voltage may differ from set voltage due to load
}

void ArduinoPin::updateInputState()
{
    // Base implementation - derived classes override this
    // Input pins read the voltage applied to them
}

double ArduinoPin::readPin() const
{
    if (isInput()) {
        return m_inputVoltage;
    } else {
        return m_outputVoltage;
    }
}

void ArduinoPin::writePin(double value)
{
    if (isOutput()) {
        m_setValue = value;
        updateOutputState();
        emit componentChanged();
    }
}

void ArduinoPin::reset()
{
    ElectricalComponent::reset();
    
    m_outputVoltage = 0.0;
    m_inputVoltage = 0.0;
    m_outputCurrent = 0.0;
    m_setValue = 0.0;
    m_isOverloaded = false;
    
    emit pinValueChanged(0.0);
}

// DigitalPin implementation
DigitalPin::DigitalPin(int pinNumber, Arduino *arduino, QObject *parent)
    : ArduinoPin(pinNumber, DIGITAL_PIN, arduino, parent)
    , m_digitalState(false)
    , m_supportsPWM(false)
    , m_pwmValue(0)
    , m_pwmTimer(nullptr)
    , m_pwmPhase(false)
{
    // Check if this pin supports PWM (pins 3, 5, 6, 9, 10, 11 on Uno)
    QList<int> pwmPins = {3, 5, 6, 9, 10, 11};
    m_supportsPWM = pwmPins.contains(pinNumber);
    
    if (m_supportsPWM) {
        setupPWMTimer();
    }
    
    setName(QString("Digital Pin %1").arg(pinNumber));
}

void DigitalPin::digitalWrite(bool value)
{
    if (m_mode != OUTPUT) {
        qWarning() << "Attempting to write to pin" << m_pinNumber << "which is not in OUTPUT mode";
        return;
    }
    
    m_digitalState = value;
    m_setValue = value ? VCC : GND;
    
    // Stop PWM if it was running
    if (m_pwmTimer && m_pwmTimer->isActive()) {
        m_pwmTimer->stop();
        m_pwmValue = 0;
    }
    
    updateOutputState();
    emit pinValueChanged(m_outputVoltage);
    emit componentChanged();
    
    qDebug() << "Digital write pin" << m_pinNumber << ":" << (value ? "HIGH" : "LOW");
}

bool DigitalPin::digitalRead() const
{
    if (!isInput()) {
        qWarning() << "Attempting to read from pin" << m_pinNumber << "which is not in input mode";
        return false;
    }
    
    // Convert analog voltage to digital reading
    double threshold = VCC / 2.0; // 2.5V threshold for 5V system
    
    if (m_mode == INPUT_PULLUP) {
        // With pullup, pin reads HIGH unless pulled low
        return m_inputVoltage > threshold;
    } else {
        return m_inputVoltage > threshold;
    }
}

void DigitalPin::analogWrite(int value)
{
    if (!m_supportsPWM) {
        qWarning() << "Pin" << m_pinNumber << "does not support PWM";
        return;
    }
    
    if (m_mode != OUTPUT) {
        qWarning() << "Pin" << m_pinNumber << "must be in OUTPUT mode for PWM";
        return;
    }
    
    m_pwmValue = std::clamp(value, 0, 255);
    m_setValue = calculatePWMVoltage();
    
    // Start PWM timer if not already running
    if (m_pwmTimer && !m_pwmTimer->isActive()) {
        m_pwmTimer->start();
    }
    
    updateOutputState();
    emit pinValueChanged(m_outputVoltage);
    emit componentChanged();
    
    qDebug() << "PWM write pin" << m_pinNumber << ":" << m_pwmValue << "(" << m_setValue << "V average)";
}

void DigitalPin::updateOutputState()
{
    if (m_mode == OUTPUT) {
        // For digital output, pin tries to output the set voltage
        m_outputVoltage = m_setValue;
    }
}

void DigitalPin::updateInputState()
{
    // Digital pin reading is handled in digitalRead()
    // This updates the internal state based on the applied voltage
}

void DigitalPin::setupPWMTimer()
{
    if (!m_pwmTimer) {
        m_pwmTimer = new QTimer(this);
        m_pwmTimer->setInterval(1000 / PWM_FREQUENCY / 2); // Half period for phase toggling
        connect(m_pwmTimer, &QTimer::timeout, this, &DigitalPin::updatePWMOutput);
    }
}

void DigitalPin::updatePWMOutput()
{
    // Toggle PWM phase
    m_pwmPhase = !m_pwmPhase;
    
    // Calculate instantaneous PWM output
    double dutyCycle = m_pwmValue / 255.0;
    double instantVoltage = m_pwmPhase && (dutyCycle > 0.5) ? VCC : GND;
    
    // For simulation purposes, we use the average voltage
    // Real hardware would show the switching, but for circuit analysis we use DC equivalent
    m_outputVoltage = calculatePWMVoltage();
    
    emit componentChanged();
}

double DigitalPin::calculatePWMVoltage() const
{
    // PWM average voltage: Vavg = (duty_cycle / 255) * VCC
    return (m_pwmValue / 255.0) * VCC;
}

// AnalogPin implementation
AnalogPin::AnalogPin(int pinNumber, Arduino *arduino, QObject *parent)
    : ArduinoPin(pinNumber, ANALOG_PIN, arduino, parent)
    , m_adcResolution(10)        // 10-bit ADC (0-1023)
    , m_referenceVoltage(VCC)    // 5V reference
    , m_lastADCReading(0)
{
    setName(QString("Analog Pin A%1").arg(pinNumber));
}

void AnalogPin::analogWrite(double voltage)
{
    if (m_mode != ANALOG_OUTPUT) {
        qWarning() << "Pin A" << m_pinNumber << "must be in ANALOG_OUTPUT mode";
        return;
    }
    
    // Clamp voltage to valid range
    voltage = std::clamp(voltage, 0.0, VCC);
    m_setValue = voltage;
    
    updateOutputState();
    emit pinValueChanged(m_outputVoltage);
    emit componentChanged();
    
    qDebug() << "Analog write pin A" << m_pinNumber << ":" << voltage << "V";
}

int AnalogPin::analogRead() const
{
    if (m_mode != ANALOG_INPUT) {
        qWarning() << "Pin A" << m_pinNumber << "must be in ANALOG_INPUT mode for reading";
        return 0;
    }
    
    return voltageToADC(m_inputVoltage);
}

void AnalogPin::updateOutputState()
{
    if (m_mode == ANALOG_OUTPUT) {
        // Analog output directly sets the pin voltage
        m_outputVoltage = m_setValue;
    }
}

void AnalogPin::updateInputState()
{
    if (m_mode == ANALOG_INPUT) {
        // Cache the ADC reading for analogRead()
        m_lastADCReading = voltageToADC(m_inputVoltage);
    }
}

int AnalogPin::voltageToADC(double voltage) const
{
    // Convert voltage to ADC reading based on resolution and reference
    int maxValue = (1 << m_adcResolution) - 1; // 2^n - 1
    double ratio = voltage / m_referenceVoltage;
    
    // Clamp to valid range
    ratio = std::clamp(ratio, 0.0, 1.0);
    
    return static_cast<int>(ratio * maxValue);
}