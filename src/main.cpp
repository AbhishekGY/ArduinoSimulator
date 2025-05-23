#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QWidget>
#include <QDebug>
#include <QTimer>

#include "simulation/Circuit.h"
#include "simulation/CircuitSimulator.h"
#include "core/Arduino.h"
#include "core/LED.h"
#include "core/Resistor.h"
#include "core/ArduinoPin.h"

// Global pointers so we can access these from UI
Circuit* g_circuit = nullptr;
CircuitSimulator* g_simulator = nullptr;
Arduino* g_arduino = nullptr;
LED* g_led = nullptr;
Resistor* g_resistor = nullptr;

// UI elements for updating
QLabel* g_statusLabel = nullptr;
QLabel* g_ledLabel = nullptr;
QLabel* g_voltageLabel = nullptr;
QLabel* g_currentLabel = nullptr;

// Update UI based on LED state
void updateLEDStatus()
{
    if (!g_led || !g_ledLabel) return;
    
    bool isOn = g_led->isOn();
    double voltage = g_led->getVoltage();
    double current = g_led->getCurrent() * 1000; // Convert to mA
    double brightness = g_led->getBrightness();
    
    if (isOn) {
        g_ledLabel->setText(QString("LED: ON (%.1f%%)").arg(brightness * 100));
        g_ledLabel->setStyleSheet("font-size: 24px; color: white; background-color: red; border-radius: 15px; padding: 10px; margin: 20px;");
    } else {
        g_ledLabel->setText("LED: OFF");
        g_ledLabel->setStyleSheet("font-size: 24px; color: black; background-color: gray; border-radius: 15px; padding: 10px; margin: 20px;");
    }
    
    if (g_voltageLabel) {
        g_voltageLabel->setText(QString("LED Voltage: %1V").arg(voltage, 0, 'f', 2));
    }
    
    if (g_currentLabel) {
        g_currentLabel->setText(QString("LED Current: %1mA").arg(current, 0, 'f', 1));
    }
    
    qDebug() << "LED Status:" << (isOn ? "ON" : "OFF") 
             << "Voltage:" << voltage << "V"
             << "Current:" << current << "mA"
             << "Brightness:" << (brightness * 100) << "%";
}

// Test function for Arduino LED circuit with simulator
bool setupArduinoLEDCircuit()
{
    qDebug() << "=== Setting up Arduino LED Circuit with Simulator ===";
    
    // Create circuit
    g_circuit = new Circuit();
    
    // Create components
    g_arduino = new Arduino(Arduino::UNO);
    g_led = LED::createStandardLED("red");
    g_resistor = new Resistor(220.0); // 220 ohm current limiting resistor
    
    // Connect components
    bool success = g_circuit->createSimpleArduinoLEDCircuit(g_arduino, g_led, g_resistor);
    
    if (!success) {
        qDebug() << "Failed to create circuit";
        return false;
    }
    
    // Create and configure simulator
    g_simulator = new CircuitSimulator(g_circuit);
    g_circuit->setSimulator(g_simulator);
    
    // Connect simulator signals for debugging and UI updates
    QObject::connect(g_simulator, &CircuitSimulator::simulationStarted, []() {
        qDebug() << "✓ Simulation started";
        if (g_statusLabel) {
            g_statusLabel->setText("Simulation running");
        }
    });
    
    QObject::connect(g_simulator, &CircuitSimulator::simulationStopped, []() {
        qDebug() << "✓ Simulation stopped";
        if (g_statusLabel) {
            g_statusLabel->setText("Simulation stopped");
        }
    });
    
    QObject::connect(g_simulator, &CircuitSimulator::convergenceAchieved, []() {
        qDebug() << "✓ Simulation converged";
        // Update UI after convergence
        QTimer::singleShot(10, updateLEDStatus);
    });
    
    QObject::connect(g_simulator, &CircuitSimulator::convergenceFailed, [](int iterations) {
        qDebug() << "✗ Simulation failed to converge after" << iterations << "iterations";
        if (g_statusLabel) {
            g_statusLabel->setText("Simulation convergence failed");
        }
    });
    
    QObject::connect(g_simulator, &CircuitSimulator::simulationError, [](const QString& error) {
        qDebug() << "✗ Simulation error:" << error;
        if (g_statusLabel) {
            g_statusLabel->setText("Simulation error: " + error);
        }
    });
    
    QObject::connect(g_simulator, &CircuitSimulator::simulationStepCompleted, 
                     [](int step, double time) {
        qDebug() << "Simulation step" << step << "completed at time" << time;
    });
    
    // Power on Arduino and configure pin
    g_arduino->powerOn();
    g_arduino->pinMode(13, Arduino::OUTPUT);
    
    // Check connection validity
    QStringList issues = g_circuit->getConnectionIssues();
    if (!issues.isEmpty()) {
        qDebug() << "Circuit issues:" << issues;
        return false;
    }
    
    // Start the simulator
    g_simulator->start();
    
    qDebug() << "Circuit and simulator set up successfully";
    return true;
}

// Clean up function
void cleanupCircuit()
{
    qDebug() << "=== Cleaning up circuit ===";
    
    // Safety check
    if (!g_arduino || !g_circuit) {
        qDebug() << "Nothing to clean up";
        return;
    }
    
    try {
        // Stop simulator first
        if (g_simulator) {
            g_simulator->stop();
            delete g_simulator;
            g_simulator = nullptr;
        }
        
        // Power off Arduino
        if (g_arduino->isPoweredOn()) {
            g_arduino->powerOff();
        }
        
        // Clear Arduino connections
        g_circuit->clearArduinoConnections(g_arduino);
        
        // Store local copies and clear globals
        Arduino* arduino = g_arduino;
        Circuit* circuit = g_circuit;
        
        g_arduino = nullptr;
        g_circuit = nullptr;
        g_led = nullptr;
        g_resistor = nullptr;
        
        // Delete components
        delete arduino;
        delete circuit;
        
        qDebug() << "✓ Circuit cleaned up successfully";
    } catch (const std::exception& e) {
        qDebug() << "Exception during cleanup:" << e.what();
    } catch (...) {
        qDebug() << "Unknown exception during cleanup";
    }
}

// Main application
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Create main window
    QMainWindow* mainWindow = new QMainWindow();
    mainWindow->setWindowTitle("Arduino Simulator - Circuit Test");
    mainWindow->resize(900, 700);
    
    // Create central widget and layout
    QWidget* centralWidget = new QWidget(mainWindow);
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    
    // Status label
    g_statusLabel = new QLabel("Circuit not created", centralWidget);
    g_statusLabel->setAlignment(Qt::AlignCenter);
    g_statusLabel->setStyleSheet("font-size: 16px; margin: 10px; padding: 10px; border: 1px solid gray;");
    layout->addWidget(g_statusLabel);
    
    // LED status indicator
    g_ledLabel = new QLabel("LED: OFF", centralWidget);
    g_ledLabel->setAlignment(Qt::AlignCenter);
    g_ledLabel->setStyleSheet("font-size: 24px; color: black; background-color: gray; border-radius: 15px; padding: 10px; margin: 20px;");
    layout->addWidget(g_ledLabel);
    
    // Voltage and current labels
    g_voltageLabel = new QLabel("LED Voltage: 0.0V", centralWidget);
    g_voltageLabel->setAlignment(Qt::AlignCenter);
    g_voltageLabel->setStyleSheet("font-size: 14px; margin: 5px;");
    layout->addWidget(g_voltageLabel);
    
    g_currentLabel = new QLabel("LED Current: 0.0mA", centralWidget);
    g_currentLabel->setAlignment(Qt::AlignCenter);
    g_currentLabel->setStyleSheet("font-size: 14px; margin: 5px;");
    layout->addWidget(g_currentLabel);
    
    // Circuit info label
    QLabel* circuitInfo = new QLabel("Circuit: Arduino Pin 13 → LED → 220Ω Resistor → Ground", centralWidget);
    circuitInfo->setAlignment(Qt::AlignCenter);
    circuitInfo->setStyleSheet("font-size: 12px; color: gray; margin: 10px;");
    layout->addWidget(circuitInfo);
    
    // Control buttons
    QPushButton* setupButton = new QPushButton("Setup Circuit & Start Simulation", centralWidget);
    QPushButton* ledOnButton = new QPushButton("digitalWrite(13, HIGH)", centralWidget);
    QPushButton* ledOffButton = new QPushButton("digitalWrite(13, LOW)", centralWidget);
    QPushButton* pwmButton = new QPushButton("analogWrite(13, 127) - 50% PWM", centralWidget);
    QPushButton* cleanupButton = new QPushButton("Cleanup Circuit", centralWidget);
    
    // Style buttons
    QString buttonStyle = "QPushButton { font-size: 14px; padding: 8px; margin: 5px; } "
                         "QPushButton:disabled { color: gray; }";
    setupButton->setStyleSheet(buttonStyle);
    ledOnButton->setStyleSheet(buttonStyle);
    ledOffButton->setStyleSheet(buttonStyle);
    pwmButton->setStyleSheet(buttonStyle);
    cleanupButton->setStyleSheet(buttonStyle);
    
    // Disable buttons initially
    ledOnButton->setEnabled(false);
    ledOffButton->setEnabled(false);
    pwmButton->setEnabled(false);
    cleanupButton->setEnabled(false);
    
    // Add buttons to layout
    layout->addWidget(setupButton);
    layout->addWidget(ledOnButton);
    layout->addWidget(ledOffButton);
    layout->addWidget(pwmButton);
    layout->addWidget(cleanupButton);
    layout->addStretch();
    
    // Button connections
    QObject::connect(setupButton, &QPushButton::clicked, [=]() {
        if (setupArduinoLEDCircuit()) {
            g_statusLabel->setText("Circuit created successfully - Simulation running");
            setupButton->setEnabled(false);
            ledOnButton->setEnabled(true);
            ledOffButton->setEnabled(true);
            pwmButton->setEnabled(true);
            cleanupButton->setEnabled(true);
            
            // Update initial status
            QTimer::singleShot(100, updateLEDStatus);
        } else {
            g_statusLabel->setText("Failed to create circuit");
        }
    });
    
    QObject::connect(ledOnButton, &QPushButton::clicked, [=]() {
        if (g_arduino) {
            qDebug() << "\n--- User clicked LED ON ---";
            g_arduino->digitalWrite(13, Arduino::HIGH);
            g_statusLabel->setText("digitalWrite(13, HIGH) - LED should turn ON");
            
            // Update will happen automatically via simulation signals
        }
    });
    
    QObject::connect(ledOffButton, &QPushButton::clicked, [=]() {
        if (g_arduino) {
            qDebug() << "\n--- User clicked LED OFF ---";
            g_arduino->digitalWrite(13, Arduino::LOW);
            g_statusLabel->setText("digitalWrite(13, LOW) - LED should turn OFF");
        }
    });
    
    /* QObject::connect(pwmButton, &QPushButton::clicked, [=]() {
        if (g_arduino) {
            qDebug() << "\n--- User clicked PWM 50% ---";
            ArduinoPin* pin13 = g_arduino->getPin(13);
            if (pin13 && pin13->supportsPWM()) {
                pin13->analogWrite(127); // 50% duty cycle
                g_statusLabel->setText("analogWrite(13, 127) - LED at 50% brightness");
            } else {
                g_statusLabel->setText("Pin 13 does not support PWM");
            }
        }
    }); */
    
    QObject::connect(cleanupButton, &QPushButton::clicked, [=]() {
        cleanupCircuit();
        g_statusLabel->setText("Circuit cleaned up");
        setupButton->setEnabled(true);
        ledOnButton->setEnabled(false);
        ledOffButton->setEnabled(false);
        pwmButton->setEnabled(false);
        cleanupButton->setEnabled(false);
        
        // Reset displays
        g_ledLabel->setText("LED: OFF");
        g_ledLabel->setStyleSheet("font-size: 24px; color: black; background-color: gray; border-radius: 15px; padding: 10px; margin: 20px;");
        g_voltageLabel->setText("LED Voltage: 0.0V");
        g_currentLabel->setText("LED Current: 0.0mA");
    });
    
    // Set central widget and show
    mainWindow->setCentralWidget(centralWidget);
    mainWindow->show();
    
    // Clean up on exit
    QObject::connect(&app, &QApplication::aboutToQuit, []() {
        if (g_circuit || g_arduino) {
            cleanupCircuit();
        }
    });
    
    qDebug() << "=== Arduino Circuit Simulator Test Ready ===";
    qDebug() << "Click 'Setup Circuit' to create the test circuit";
    
    return app.exec();
}