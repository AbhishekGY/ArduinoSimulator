#ifndef ARDUINO_H
#define ARDUINO_H

#include <QObject>
#include <QVector>
#include <QHash>
#include <QTimer>
#include <QString>
#include "ArduinoPin.h"

class Circuit;

class Arduino : public QObject
{
    Q_OBJECT

public:
    enum BoardType {
        UNO,
        NANO,
        MEGA
    };

    // Pin mode constants
    static const int INPUT = 0;
    static const int OUTPUT = 1;
    static const int INPUT_PULLUP = 2;

    // Digital state constants  
    static const int LOW = 0;
    static const int HIGH = 1;

    // Analog reference types
    static const int DEFAULT = 1;
    static const int INTERNAL = 3;
    static const int EXTERNAL = 0;

    explicit Arduino(BoardType board = UNO, QObject *parent = nullptr);
    ~Arduino();

    // Board information
    BoardType getBoardType() const { return m_boardType; }
    QString getBoardName() const;
    int getDigitalPinCount() const { return m_digitalPins.size(); }
    int getAnalogPinCount() const { return m_analogPins.size(); }

    // Pin access
    DigitalPin* getDigitalPin(int pin);
    AnalogPin* getAnalogPin(int pin);
    ArduinoPin* getPin(int pin); // Gets digital pin by number
    QVector<ArduinoPin*> getAllPins() const;

    // Arduino sketch functions
    void pinMode(int pin, int mode);
    void digitalWrite(int pin, int value);
    int digitalRead(int pin);
    void analogWrite(int pin, int value);
    int analogRead(int pin);

    // Advanced pin functions
    void analogReference(int type);
    unsigned long pulseIn(int pin, int value, unsigned long timeout = 1000000);

    // Timing functions (for sketch simulation)
    unsigned long millis() const;
    unsigned long micros() const;
    void delay(unsigned long ms);
    void delayMicroseconds(unsigned int us);

    // Circuit integration
    void setCircuit(Circuit *circuit);
    Circuit* getCircuit() const { return m_circuit; }

    // Power and reset
    void powerOn();
    void powerOff();
    void reset();
    bool isPoweredOn() const { return m_isPoweredOn; }

    // Status and monitoring
    double getSupplyVoltage() const { return m_supplyVoltage; }
    double getSupplyCurrent() const;
    bool isOverloaded() const;
    QVector<ArduinoPin*> getOverloadedPins() const;

    // Special pins
    ArduinoPin* getGroundPin() const { return m_groundPin; }
    ArduinoPin* getVccPin() const { return m_vccPin; }

    // sketch simulation
    void loadSketch(const QString &sketchCode);
    void startSketch();
    void stopSketch();
    bool isSketchRunning() const { return m_sketchRunning; }

signals:
    void pinModeChanged(int pin, int mode);
    void pinValueChanged(int pin, double value);
    void arduinoPowered(bool on);
    void arduinoReset();
    void sketchStarted();
    void sketchStopped();
    void overloadDetected(int pin);

private slots:
    void onPinModeChanged(ArduinoPin::PinMode mode);
    void onPinValueChanged(double value);
    void onPinOverloaded();
    void updateSketch();

private:
    void initializePins();
    void createPin(int pinNumber, bool isAnalog = false);
    DigitalPin* findDigitalPin(int pin);
    AnalogPin* findAnalogPin(int pin);

    // Board configuration
    BoardType m_boardType;
    int m_digitalPinCount;
    int m_analogPinCount;
    QVector<int> m_pwmPins;

    // Pin management
    QVector<DigitalPin*> m_digitalPins;
    QVector<AnalogPin*> m_analogPins;
    QHash<int, ArduinoPin*> m_pinMap; // Quick lookup by pin number

    // Special pins
    ArduinoPin *m_groundPin;
    ArduinoPin *m_vccPin;

    // Circuit integration
    Circuit *m_circuit;

    // Power management
    bool m_isPoweredOn;
    double m_supplyVoltage;
    double m_maxTotalCurrent;

    // Timing simulation
    QTimer *m_systemTimer;
    unsigned long m_startTime;

    // Sketch simulation
    QString m_sketchCode;
    bool m_sketchRunning;
    QTimer *m_sketchTimer;
    int m_sketchLoopDelay;

    // Analog reference
    double m_analogReference;
};

// Helper class for special Arduino pins (VCC, GND)
class ArduinoPowerPin : public ArduinoPin
{
    Q_OBJECT

public:
    enum PowerType {
        GROUND,
        VCC_5V,
        VCC_3V3
    };

    explicit ArduinoPowerPin(PowerType type, Arduino *arduino, QObject *parent = nullptr);

    double getResistance() const override;
    void updateState(double voltage, double current) override;

private:
    PowerType m_powerType;
    double m_fixedVoltage;
};

#endif // ARDUINO_H