#ifndef LED_H
#define LED_H

#include "ElectricalComponent.h"
#include <QColor>

class LED : public ElectricalComponent
{
    Q_OBJECT

public:
    explicit LED(const QColor &color = Qt::red, QObject *parent = nullptr);

    // Electrical characteristics
    double getResistance() const override;
    void updateState(double voltage, double current) override;

    // LED properties
    bool isOn() const { return m_isOn; }
    double getBrightness() const { return m_brightness; }
    const QColor &getColor() const { return m_color; }
    void setColor(const QColor &color);

    // Electrical parameters
    double getForwardVoltage() const { return m_forwardVoltage; }
    void setForwardVoltage(double voltage);
    
    double getMaxCurrent() const { return m_maxCurrent; }
    void setMaxCurrent(double current);

    // Power and thermal calculations
    double getPowerDissipation() const;
    bool isOverloaded() const { return m_isOverloaded; }

    // Reset to initial state
    void reset() override;

    // Factory method for common LED types
    static LED* createStandardLED(const QString &type, QObject *parent = nullptr);

signals:
    void ledStateChanged(bool isOn, double brightness);
    void overloadDetected();

private:
    void calculateElectricalState();
    void checkOverloadCondition();
    double calculateBrightness(double current) const;
    double calculateDynamicResistance(double current) const;

    // Visual properties
    bool m_isOn;
    double m_brightness;        // 0.0 to 1.0
    QColor m_color;

    // Electrical characteristics
    double m_forwardVoltage;    // Threshold voltage for conduction
    double m_forwardCurrent;    // Typical operating current
    double m_maxCurrent;        // Maximum safe current
    double m_dynamicResistance; // Current-dependent resistance
    
    // Protection
    bool m_isOverloaded;
    double m_thermalLimit;      // Maximum power dissipation
    
    // LED model parameters
    static constexpr double OFF_RESISTANCE = 1e6;    // Very high when off
    static constexpr double MIN_CONDUCTION_CURRENT = 1e-6; // Minimum detectable current
    static constexpr double THERMAL_VOLTAGE = 0.026; // kT/q at room temperature
};

#endif // LED_H