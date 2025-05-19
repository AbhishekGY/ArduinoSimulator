#include "core/Arduino.h"
#include "core/ArduinoPin.h"
#include "simulation/Circuit.h"
#include <QDebug>
#include <QTime>
#include <algorithm>

Arduino::Arduino(BoardType board, QObject *parent)
    : QObject(parent)
    , m_boardType(board)
    , m_circuit(nullptr)
    , m_groundPin(nullptr)
    , m_vccPin(nullptr)
    , m_isPoweredOn(false)
    , m_supplyVoltage(5.0)
    , m_maxTotalCurrent(0.5) // 500mA total limit
    , m_systemTimer(nullptr)
    , m_startTime(0)
    , m_sketchRunning(false)
    , m_sketchTimer(nullptr)
    , m_sketchLoopDelay(1) // 1ms default loop delay
    , m_analogReference(5.0)
{
    // Configure board-specific parameters
    switch (m_boardType) {
        case UNO:
            m_digitalPinCount = 14; // D0-D13
            m_analogPinCount = 6;   // A0-A5
            m_pwmPins = {3, 5, 6, 9, 10, 11};
            break;
            
        case NANO:
            m_digitalPinCount = 14; // D0-D13
            m_analogPinCount = 8;   // A0-A7
            m_pwmPins = {3, 5, 6, 9, 10, 11};
            break;
            
        case MEGA:
            m_digitalPinCount = 54; // D0-D53
            m_analogPinCount = 16;  // A0-A15
            m_pwmPins = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
            break;
    }

    // Initialize timing system
    m_systemTimer = new QTimer(this);
    m_systemTimer->setInterval(1); // 1ms resolution
    connect(m_systemTimer, &QTimer::timeout, this, &Arduino::updateSketch);
    m_startTime = QTime::currentTime().msecsSinceStartOfDay();

    // Initialize sketch timer
    m_sketchTimer = new QTimer(this);
    connect(m_sketchTimer, &QTimer::timeout, this, &Arduino::updateSketch);

    // Create pins
    initializePins();

    qDebug() << "Arduino" << getBoardName() << "initialized with" 
             << m_digitalPinCount << "digital pins and" << m_analogPinCount << "analog pins";
}

Arduino::~Arduino()
{
    qDeleteAll(m_digitalPins);
    qDeleteAll(m_analogPins);
    delete m_groundPin;
    delete m_vccPin;
}

QString Arduino::getBoardName() const
{
    switch (m_boardType) {
        case UNO: return "Arduino Uno";
        case NANO: return "Arduino Nano";
        case MEGA: return "Arduino Mega";
        default: return "Unknown Arduino";
    }
}

void Arduino::initializePins()
{
    // Create digital pins
    for (int i = 0; i < m_digitalPinCount; ++i) {
        createPin(i, false);
    }

    // Create analog pins
    for (int i = 0; i < m_analogPinCount; ++i) {
        createPin(i, true);
    }

    // Create special power pins
    m_groundPin = new ArduinoPowerPin(ArduinoPowerPin::GROUND, this, this);
    m_vccPin = new ArduinoPowerPin(ArduinoPowerPin::VCC_5V, this, this);

    qDebug() << "Created" << m_digitalPins.size() << "digital pins and" 
             << m_analogPins.size() << "analog pins";
}

void Arduino::createPin(int pinNumber, bool isAnalog)
{
    ArduinoPin *pin = nullptr;

    if (isAnalog) {
        AnalogPin *analogPin = new AnalogPin(pinNumber, this, this);
        m_analogPins.append(analogPin);
        pin = analogPin;
    } else {
        DigitalPin *digitalPin = new DigitalPin(pinNumber, this, this);
        m_digitalPins.append(digitalPin);
        pin = digitalPin;
    }

    // Connect pin signals
    connect(pin, QOverload<ArduinoPin::PinMode>::of(&ArduinoPin::pinModeChanged),
            this, &Arduino::onPinModeChanged);
    connect(pin, &ArduinoPin::pinValueChanged, this, &Arduino::onPinValueChanged);
    connect(pin, &ArduinoPin::pinOverloaded, this, &Arduino::onPinOverloaded);

    // Add to quick lookup map
    m_pinMap[pinNumber + (isAnalog ? 1000 : 0)] = pin; // Use 1000+ for analog pins
}

DigitalPin* Arduino::getDigitalPin(int pin)
{
    return findDigitalPin(pin);
}

AnalogPin* Arduino::getAnalogPin(int pin)
{
    return findAnalogPin(pin);
}

ArduinoPin* Arduino::getPin(int pin)
{
    return findDigitalPin(pin);
}

QVector<ArduinoPin*> Arduino::getAllPins() const
{
    QVector<ArduinoPin*> allPins;
    
    for (DigitalPin *pin : m_digitalPins) {
        allPins.append(pin);
    }
    for (AnalogPin *pin : m_analogPins) {
        allPins.append(pin);
    }
    
    if (m_groundPin) allPins.append(m_groundPin);
    if (m_vccPin) allPins.append(m_vccPin);
    
    return allPins;
}

DigitalPin* Arduino::findDigitalPin(int pin)
{
    if (pin >= 0 && pin < m_digitalPins.size()) {
        return m_digitalPins[pin];
    }
    return nullptr;
}

AnalogPin* Arduino::findAnalogPin(int pin)
{
    if (pin >= 0 && pin < m_analogPins.size()) {
        return m_analogPins[pin];
    }
    return nullptr;
}

// Arduino sketch function implementations
void Arduino::pinMode(int pin, int mode)
{
    if (!m_isPoweredOn) {
        qWarning() << "Arduino not powered on";
        return;
    }

    DigitalPin *digitalPin = findDigitalPin(pin);
    if (digitalPin) {
        ArduinoPin::PinMode pinMode;
        switch (mode) {
            case INPUT: pinMode = ArduinoPin::INPUT; break;
            case OUTPUT: pinMode = ArduinoPin::OUTPUT; break;
            case INPUT_PULLUP: pinMode = ArduinoPin::INPUT_PULLUP; break;
            default:
                qWarning() << "Invalid pin mode:" << mode;
                return;
        }
        
        digitalPin->setMode(pinMode);
        emit pinModeChanged(pin, mode);
        
        qDebug() << "pinMode(" << pin << "," << mode << ")";
    } else {
        qWarning() << "Invalid digital pin:" << pin;
    }
}

void Arduino::digitalWrite(int pin, int value)
{
    if (!m_isPoweredOn) return;

    DigitalPin *digitalPin = findDigitalPin(pin);
    if (digitalPin) {
        digitalPin->digitalWrite(value == HIGH);
        qDebug() << "digitalWrite(" << pin << "," << (value ? "HIGH" : "LOW") << ")";
    } else {
        qWarning() << "Invalid digital pin for write:" << pin;
    }
}

int Arduino::digitalRead(int pin)
{
    if (!m_isPoweredOn) return LOW;

    DigitalPin *digitalPin = findDigitalPin(pin);
    if (digitalPin) {
        bool state = digitalPin->digitalRead();
        qDebug() << "digitalRead(" << pin << ") =" << (state ? "HIGH" : "LOW");
        return state ? HIGH : LOW;
    } else {
        qWarning() << "Invalid digital pin for read:" << pin;
        return LOW;
    }
}

void Arduino::analogWrite(int pin, int value)
{
    if (!m_isPoweredOn) return;

    DigitalPin *digitalPin = findDigitalPin(pin);
    if (digitalPin && digitalPin->supportsPWM()) {
        digitalPin->analogWrite(value);
        qDebug() << "analogWrite(" << pin << "," << value << ")";
    } else if (digitalPin) {
        qWarning() << "Pin" << pin << "does not support PWM";
    } else {
        qWarning() << "Invalid pin for analogWrite:" << pin;
    }
}

int Arduino::analogRead(int pin)
{
    if (!m_isPoweredOn) return 0;

    AnalogPin *analogPin = findAnalogPin(pin);
    if (analogPin) {
        int reading = analogPin->analogRead();
        qDebug() << "analogRead(A" << pin << ") =" << reading;
        return reading;
    } else {
        qWarning() << "Invalid analog pin:" << pin;
        return 0;
    }
}

void Arduino::analogReference(int type)
{
    switch (type) {
        case DEFAULT:
            m_analogReference = 5.0;
            break;
        case INTERNAL:
            m_analogReference = 1.1; // Internal 1.1V reference
            break;
        case EXTERNAL:
            m_analogReference = 3.3; // Assume external 3.3V reference
            break;
        default:
            qWarning() << "Invalid analog reference type:" << type;
            return;
    }

    // Update all analog pins with new reference
    for (AnalogPin *pin : m_analogPins) {
        pin->setReference(m_analogReference);
    }

    qDebug() << "analogReference set to" << m_analogReference << "V";
}

unsigned long Arduino::pulseIn(int pin, int value, unsigned long timeout)
{
    // Simplified implementation - in real simulation this would measure actual pulse timing
    Q_UNUSED(pin)
    Q_UNUSED(value)
    Q_UNUSED(timeout)
    
    qWarning() << "pulseIn() not fully implemented in simulation";
    return 0;
}

// Timing functions
unsigned long Arduino::millis() const
{
    if (!m_isPoweredOn) return 0;
    return QTime::currentTime().msecsSinceStartOfDay() - m_startTime;
}

unsigned long Arduino::micros() const
{
    if (!m_isPoweredOn) return 0;
    return (QTime::currentTime().msecsSinceStartOfDay() - m_startTime) * 1000;
}

void Arduino::delay(unsigned long ms)
{
    if (!m_isPoweredOn) return;
    
    // In real implementation, this would pause sketch execution
    // For simulation, we just log the delay
    qDebug() << "delay(" << ms << ")";
}

void Arduino::delayMicroseconds(unsigned int us)
{
    if (!m_isPoweredOn) return;
    
    qDebug() << "delayMicroseconds(" << us << ")";
}

// Circuit integration
void Arduino::setCircuit(Circuit *circuit)
{
    m_circuit = circuit;
    
    // Add all pins to the circuit
    for (ArduinoPin *pin : getAllPins()) {
        if (circuit) {
            circuit->addComponent(pin);
        }
    }
}

// Power management
void Arduino::powerOn()
{
    if (!m_isPoweredOn) {
        m_isPoweredOn = true;
        m_startTime = QTime::currentTime().msecsSinceStartOfDay();
        
        if (m_systemTimer) {
            m_systemTimer->start();
        }
        
        emit arduinoPowered(true);
        qDebug() << "Arduino powered on";
    }
}

void Arduino::powerOff()
{
    if (m_isPoweredOn) {
        m_isPoweredOn = false;
        
        stopSketch();
        
        if (m_systemTimer) {
            m_systemTimer->stop();
        }
        
        // Reset all pins
        for (ArduinoPin *pin : getAllPins()) {
            pin->reset();
        }
        
        emit arduinoPowered(false);
        qDebug() << "Arduino powered off";
    }
}

void Arduino::reset()
{
    qDebug() << "Arduino reset";
    
    // Stop sketch
    stopSketch();
    
    // Reset all pins to INPUT mode
    for (DigitalPin *pin : m_digitalPins) {
        pin->setMode(ArduinoPin::INPUT);
        pin->reset();
    }
    
    for (AnalogPin *pin : m_analogPins) {
        pin->setMode(ArduinoPin::ANALOG_INPUT);
        pin->reset();
    }
    
    // Reset timing
    m_startTime = QTime::currentTime().msecsSinceStartOfDay();
    
    emit arduinoReset();
}

double Arduino::getSupplyCurrent() const
{
    double totalCurrent = 0.0;
    
    for (ArduinoPin *pin : getAllPins()) {
        if (pin->isOutput()) {
            totalCurrent += std::abs(pin->getCurrent());
        }
    }
    
    return totalCurrent;
}

bool Arduino::isOverloaded() const
{
    return getSupplyCurrent() > m_maxTotalCurrent;
}

QVector<ArduinoPin*> Arduino::getOverloadedPins() const
{
    QVector<ArduinoPin*> overloadedPins;
    
    for (ArduinoPin *pin : getAllPins()) {
        if (pin->isOverloaded()) {
            overloadedPins.append(pin);
        }
    }
    
    return overloadedPins;
}

// Sketch simulation
void Arduino::loadSketch(const QString &sketchCode)
{
    m_sketchCode = sketchCode;
    qDebug() << "Sketch loaded:" << sketchCode.length() << "characters";
}

void Arduino::startSketch()
{
    if (!m_isPoweredOn) {
        qWarning() << "Cannot start sketch - Arduino not powered";
        return;
    }

    if (!m_sketchRunning) {
        m_sketchRunning = true;
        
        if (m_sketchTimer) {
            m_sketchTimer->start(m_sketchLoopDelay);
        }
        
        emit sketchStarted();
        qDebug() << "Sketch started";
    }
}

void Arduino::stopSketch()
{
    if (m_sketchRunning) {
        m_sketchRunning = false;
        
        if (m_sketchTimer) {
            m_sketchTimer->stop();
        }
        
        emit sketchStopped();
        qDebug() << "Sketch stopped";
    }
}

// Slot implementations
void Arduino::onPinModeChanged(ArduinoPin::PinMode mode)
{
    ArduinoPin *pin = qobject_cast<ArduinoPin*>(sender());
    if (pin) {
        emit pinModeChanged(pin->getPinNumber(), static_cast<int>(mode));
    }
}

void Arduino::onPinValueChanged(double value)
{
    ArduinoPin *pin = qobject_cast<ArduinoPin*>(sender());
    if (pin) {
        emit pinValueChanged(pin->getPinNumber(), value);
    }
}

void Arduino::onPinOverloaded()
{
    ArduinoPin *pin = qobject_cast<ArduinoPin*>(sender());
    if (pin) {
        emit overloadDetected(pin->getPinNumber());
    }
}

void Arduino::updateSketch()
{
    // Placeholder for sketch execution simulation
    // In a full implementation, this would execute the compiled sketch bytecode
    if (m_sketchRunning) {
        // Execute one iteration of the sketch loop
        // For now, just maintain the sketch running state
    }
}

// ArduinoPowerPin implementation
ArduinoPowerPin::ArduinoPowerPin(PowerType type, Arduino *arduino, QObject *parent)
    : ArduinoPin(-1, ArduinoPin::DIGITAL_PIN, arduino, parent)
    , m_powerType(type)
{
    switch (type) {
        case GROUND:
            m_fixedVoltage = 0.0;
            setName("GND");
            break;
        case VCC_5V:
            m_fixedVoltage = 5.0;
            setName("5V");
            break;
        case VCC_3V3:
            m_fixedVoltage = 3.3;
            setName("3.3V");
            break;
    }
    
    // Power pins are always "output" pins with fixed voltage
    setMode(ArduinoPin::OUTPUT);
}

double ArduinoPowerPin::getResistance() const
{
    // Power pins have very low resistance
    return 0.01; // 10mΩ
}

void ArduinoPowerPin::updateState(double voltage, double current)
{
    // Power pins maintain their fixed voltage regardless of load
    ArduinoPin::updateState(m_fixedVoltage, current);
    m_outputVoltage = m_fixedVoltage;
}