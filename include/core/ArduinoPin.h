#ifndef ARDUINOPIN_H
#define ARDUINOPIN_H

#include "ElectricalComponent.h"
#include <QTimer>

class Arduino;

// Base class for all Arduino pins
class ArduinoPin : public ElectricalComponent
{
    Q_OBJECT

public:
    enum PinMode {
        INPUT,
        OUTPUT,
        INPUT_PULLUP,
        ANALOG_INPUT,
        ANALOG_OUTPUT
    };

    enum PinType {
        DIGITAL_PIN,
        ANALOG_PIN,
        PWM_PIN
    };

    explicit ArduinoPin(int pinNumber, PinType type, Arduino *arduino, QObject *parent = nullptr);

    // Pin identification
    int getPinNumber() const { return m_pinNumber; }
    PinType getPinType() const { return m_pinType; }
    
    // Pin configuration
    PinMode getMode() const { return m_mode; }
    virtual void setMode(PinMode mode);
    
    // Pin state
    bool isOutput() const { return m_mode == OUTPUT || m_mode == ANALOG_OUTPUT; }
    bool isInput() const { return m_mode == INPUT || m_mode == INPUT_PULLUP || m_mode == ANALOG_INPUT; }
    
    // Electrical properties
    double getVoltage() const override { return m_outputVoltage; }
    double getCurrent() const override { return m_outputCurrent; }
    double getResistance() const override;
    
    // Protection and limits
    double getMaxCurrent() const { return m_maxCurrent; }
    void setMaxCurrent(double current) { m_maxCurrent = current; }
    bool isOverloaded() const { return m_isOverloaded; }
    
    // Arduino integration
    Arduino* getArduino() const { return m_arduino; }
    
    // Simulation updates
    void updateState(double voltage, double current) override;
    void reset() override;

    // Pin reading/writing (for sketch simulation)
    virtual double readPin() const;
    virtual void writePin(double value);

signals:
    void pinModeChanged(PinMode mode);
    void pinValueChanged(double value);
    void pinOverloaded();

protected:
    void checkOverloadCondition();
    virtual void updateOutputState();
    virtual void updateInputState();

    // Pin properties
    int m_pinNumber;
    PinType m_pinType;
    PinMode m_mode;
    Arduino *m_arduino;
    
    // Electrical state
    double m_outputVoltage;     // Voltage being output by pin
    double m_inputVoltage;      // Voltage being read on pin
    double m_outputCurrent;     // Current sourced/sunk by pin
    double m_setValue;          // Value set by sketch (digitalWrite/analogWrite)
    
    // Pin characteristics
    double m_maxCurrent;        // Maximum safe current (typically 40mA)
    double m_outputResistance;  // Internal resistance when outputting
    double m_inputResistance;   // Input impedance when reading
    bool m_isOverloaded;
    
    // Supply voltages
    static constexpr double VCC = 5.0;  // Arduino supply voltage
    static constexpr double GND = 0.0;  // Ground reference
};

// Digital pin implementation
class DigitalPin : public ArduinoPin
{
    Q_OBJECT

public:
    explicit DigitalPin(int pinNumber, Arduino *arduino, QObject *parent = nullptr);

    // Digital pin operations
    void digitalWrite(bool value);
    bool digitalRead() const;
    
    // PWM support (for pins that support it)
    bool supportsPWM() const { return m_supportsPWM; }
    void analogWrite(int value); // 0-255 PWM value
    int getPWMValue() const { return m_pwmValue; }
    
    // Pin state
    bool getDigitalState() const { return m_digitalState; }

protected:
    void updateOutputState() override;
    void updateInputState() override;

private slots:
    void updatePWMOutput();

private:
    void setupPWMTimer();
    double calculatePWMVoltage() const;

    // Digital state
    bool m_digitalState;        // HIGH/LOW state
    bool m_supportsPWM;         // Whether this pin supports PWM
    
    // PWM implementation
    int m_pwmValue;             // 0-255 PWM duty cycle
    QTimer *m_pwmTimer;         // Timer for PWM simulation
    bool m_pwmPhase;            // Current PWM output phase
    static constexpr int PWM_FREQUENCY = 1000; // 1kHz PWM frequency
};

// Analog pin implementation  
class AnalogPin : public ArduinoPin
{
    Q_OBJECT

public:
    explicit AnalogPin(int pinNumber, Arduino *arduino, QObject *parent = nullptr);

    // Analog operations
    void analogWrite(double voltage); // 0-5V output
    int analogRead() const;           // 0-1023 ADC reading
    
    // ADC properties
    int getADCResolution() const { return m_adcResolution; }
    void setADCResolution(int bits) { m_adcResolution = bits; }
    
    // Reference voltage
    double getReference() const { return m_referenceVoltage; }
    void setReference(double voltage) { m_referenceVoltage = voltage; }

protected:
    void updateOutputState() override;
    void updateInputState() override;

private:
    int voltageToADC(double voltage) const;
    
    // Analog properties
    int m_adcResolution;        // ADC resolution in bits (default 10)
    double m_referenceVoltage;  // ADC reference voltage (default 5V)
    int m_lastADCReading;       // Cached ADC reading
};

#endif // ARDUINOPIN_H