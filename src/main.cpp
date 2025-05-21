#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QWidget>
#include <QDebug>

#include "simulation/Circuit.h"
#include "core/Arduino.h"
#include "core/LED.h"
#include "core/Resistor.h"
#include "core/ArduinoPin.h"


// Global pointers so we can access these from UI
Circuit* g_circuit = nullptr;
Arduino* g_arduino = nullptr;
LED* g_led = nullptr;
Resistor* g_resistor = nullptr;

// Test function for Arduino LED circuit with proper cleanup
bool setupArduinoLEDCircuit()
{
    // Create circuit
    g_circuit = new Circuit();
    
    // Create components
    g_arduino = new Arduino(Arduino::UNO);
    g_led = LED::createStandardLED("red");
    g_resistor = new Resistor(220.0); // 220 ohm current limiting resistor
    
    // Connect components with the corrected API
    bool success = g_circuit->createSimpleArduinoLEDCircuit(g_arduino, g_led, g_resistor);
    
    if (success) {
        qDebug() << "Circuit created successfully";
        g_arduino->powerOn();
        g_arduino->pinMode(13, Arduino::OUTPUT);
        
        // Check connection validity
        QStringList issues = g_circuit->getConnectionIssues();
        if (!issues.isEmpty()) {
            qDebug() << "Circuit issues:" << issues;
            return false;
        }
        
        return true;
    } else {
        qDebug() << "Failed to create circuit";
        return false;
    }
}

// Clean up function for proper cleanup
void cleanupCircuit()
{
    // Safety check
    if (!g_arduino || !g_circuit) {
        qDebug() << "Nothing to clean up";
        return;
    }
    
    try {
        // Stop simulation if running
        if (g_circuit->isSimulationRunning()) {
            g_circuit->stopSimulation();
        }
        
        // Power off Arduino
        if (g_arduino->isPoweredOn()) {
            g_arduino->powerOff();
        }
        
        // Use our safer circuit method to handle Arduino pins properly
        g_circuit->clearArduinoConnections(g_arduino);
        
        // Store local copies of pointers
        Arduino* arduino = g_arduino;
        Circuit* circuit = g_circuit;
        
        // Clear global pointers first to prevent use-after-free
        g_arduino = nullptr;
        g_circuit = nullptr;
        g_led = nullptr;
        g_resistor = nullptr;
        
        // Delete Arduino first (which will clean up its pins)
        qDebug() << "Deleting Arduino";
        delete arduino;
        
        // Then delete circuit (which will clean up other components)
        qDebug() << "Deleting Circuit";
        delete circuit;
        
        qDebug() << "Circuit cleaned up successfully";
    } catch (const std::exception& e) {
        qDebug() << "Exception during cleanup:" << e.what();
    } catch (...) {
        qDebug() << "Unknown exception during cleanup";
    }
}

// Main application with interactive UI
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Create main window
    QMainWindow* mainWindow = new QMainWindow();
    mainWindow->setWindowTitle("Arduino Simulator - Test");
    mainWindow->resize(800, 600);
    
    // Create central widget and layout
    QWidget* centralWidget = new QWidget(mainWindow);
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    
    // Add a status label
    QLabel* statusLabel = new QLabel("Circuit not created", centralWidget);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet("font-size: 18px; margin: 20px;");
    layout->addWidget(statusLabel);
    
    // Add LED status indicator
    QLabel* ledLabel = new QLabel("LED: OFF", centralWidget);
    ledLabel->setAlignment(Qt::AlignCenter);
    ledLabel->setStyleSheet("font-size: 24px; color: black; background-color: gray; border-radius: 15px; padding: 10px; margin: 20px;");
    layout->addWidget(ledLabel);
    
    // Add buttons for circuit control
    QPushButton* setupButton = new QPushButton("Setup Circuit", centralWidget);
    QPushButton* ledOnButton = new QPushButton("Turn LED ON", centralWidget);
    QPushButton* ledOffButton = new QPushButton("Turn LED OFF", centralWidget);
    QPushButton* cleanupButton = new QPushButton("Cleanup Circuit", centralWidget);
    
    // Disable buttons initially
    ledOnButton->setEnabled(false);
    ledOffButton->setEnabled(false);
    cleanupButton->setEnabled(false);
    
    // Add buttons to layout
    layout->addWidget(setupButton);
    layout->addWidget(ledOnButton);
    layout->addWidget(ledOffButton);
    layout->addWidget(cleanupButton);
    
    // Add spacer
    layout->addStretch();
    
    // Button connections
    QObject::connect(setupButton, &QPushButton::clicked, [=]() {
        if (setupArduinoLEDCircuit()) {
            statusLabel->setText("Circuit created successfully");
            setupButton->setEnabled(false);
            ledOnButton->setEnabled(true);
            ledOffButton->setEnabled(true);
            cleanupButton->setEnabled(true);
        } else {
            statusLabel->setText("Failed to create circuit");
        }
    });
    
    QObject::connect(ledOnButton, &QPushButton::clicked, [=]() {
        if (g_arduino) {
            g_arduino->digitalWrite(13, Arduino::HIGH);
            statusLabel->setText("digitalWrite(13, HIGH) called");
            
            ArduinoPin* pin13 = g_arduino->getPin(13);
            double voltage = pin13 ? pin13->getVoltage() : 0.0;
            qDebug() << "Pin 13 voltage:" << voltage << "V";
            
            // In a real circuit simulator, the LED would be lit here
            // For now, we'll just update the UI
            ledLabel->setText("LED: ON (waiting for simulator)");
            ledLabel->setStyleSheet("font-size: 24px; color: white; background-color: red; border-radius: 15px; padding: 10px; margin: 20px;");
        }
    });
    
    QObject::connect(ledOffButton, &QPushButton::clicked, [=]() {
        if (g_arduino) {
            g_arduino->digitalWrite(13, Arduino::LOW);
            statusLabel->setText("digitalWrite(13, LOW) called");
            
            ArduinoPin* pin13 = g_arduino->getPin(13);
            double voltage = pin13 ? pin13->getVoltage() : 0.0;
            qDebug() << "Pin 13 voltage:" << voltage << "V";
            
            // Update UI
            ledLabel->setText("LED: OFF");
            ledLabel->setStyleSheet("font-size: 24px; color: black; background-color: gray; border-radius: 15px; padding: 10px; margin: 20px;");
        }
    });
    
    QObject::connect(cleanupButton, &QPushButton::clicked, [=]() {
        cleanupCircuit();
        statusLabel->setText("Circuit cleaned up");
        setupButton->setEnabled(true);
        ledOnButton->setEnabled(false);
        ledOffButton->setEnabled(false);
        cleanupButton->setEnabled(false);
        ledLabel->setText("LED: OFF");
        ledLabel->setStyleSheet("font-size: 24px; color: black; background-color: gray; border-radius: 15px; padding: 10px; margin: 20px;");
    });
    
    // Set central widget
    mainWindow->setCentralWidget(centralWidget);
    
    // Show window
    mainWindow->show();
    
    // Clean up memory when application exits
    QObject::connect(&app, &QApplication::aboutToQuit, []() {
        if (g_circuit || g_arduino) {
            cleanupCircuit();
        }
    });
    
    return app.exec();
}