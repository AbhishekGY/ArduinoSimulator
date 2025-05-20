#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QLabel>
#include <QWidget>
#include <QDebug>

#include "simulation/Circuit.h"
#include "core/Arduino.h"
#include "core/LED.h"
#include "core/Resistor.h"
#include "core/ArduinoPin.h"

void testArduinoLEDCircuit()
{
    // Create circuit
    Circuit* circuit = new Circuit();
    
    // Create components
    Arduino* arduino = new Arduino(Arduino::UNO);
    LED* led = LED::createStandardLED("red");
    Resistor* resistor = new Resistor(220.0); // 220 ohm current limiting resistor
    
    // Connect components with the corrected API
    bool success = circuit->createSimpleArduinoLEDCircuit(arduino, led, resistor);
    
    if (success) {
        qDebug() << "Circuit created successfully";
        
        // Power on Arduino
        arduino->powerOn();
        
        // Set pin 13 as output
        arduino->pinMode(13, Arduino::OUTPUT);
        
        // Test the circuit flow:
        // digitalWrite(13, HIGH) → Pin 13 voltage = 5V
        arduino->digitalWrite(13, Arduino::HIGH);
        
        // At this point, the Arduino pin should output 5V
        ArduinoPin* pin13 = arduino->getPin(13);
        qDebug() << "Pin 13 voltage after digitalWrite HIGH:" << pin13->getVoltage() << "V";
        
        // Check LED state - note that without the CircuitSimulator, 
        // the voltage won't propagate automatically
        qDebug() << "LED state:" << (led->isOn() ? "ON" : "OFF");
        qDebug() << "LED voltage:" << led->getVoltage() << "V";
        
        // For now, check connection validity
        QStringList issues = circuit->getConnectionIssues();
        if (issues.isEmpty()) {
            qDebug() << "Circuit connections are valid";
        } else {
            qDebug() << "Circuit issues:" << issues;
        }
        
        // Test turning LED off
        arduino->digitalWrite(13, Arduino::LOW);
        qDebug() << "Pin 13 voltage after digitalWrite LOW:" << pin13->getVoltage() << "V";
        
    } else {
        qDebug() << "Failed to create circuit";
    }
    
    // Cleanup
    delete circuit; // This will delete all components except Arduino
    delete arduino; // Need to delete Arduino separately
}

// Alternative manual connection approach for more control
void testManualArduinoLEDConnection()
{
    Circuit* circuit = new Circuit();
    
    // Create components
    Arduino* arduino = new Arduino(Arduino::UNO);
    LED* led = LED::createStandardLED("red");
    Resistor* resistor = new Resistor(220.0);
    
    // Add LED and resistor to circuit
    circuit->addComponent(led);
    circuit->addComponent(resistor);
    
    // Manual connection approach:
    
    // 1. Create nodes for circuit
    Node* pin13Node = circuit->createNode();
    Node* ledCathodeNode = circuit->createNode();
    Node* groundNode = circuit->getGroundNode();
    
    // 2. Connect Arduino pin 13 to LED anode
    circuit->connectArduinoPinByName(arduino, "13", pin13Node);
    circuit->connectComponentToNode(led, 0, pin13Node); // LED anode
    
    // 3. Connect LED cathode to resistor
    circuit->connectComponentToNode(led, 1, ledCathodeNode); // LED cathode  
    circuit->connectComponentToNode(resistor, 0, ledCathodeNode);
    
    // 4. Connect resistor to ground
    circuit->connectComponentToNode(resistor, 1, groundNode);
    
    // 5. Connect Arduino ground pin
    circuit->connectArduinoPinByName(arduino, "GND", groundNode);
    
    qDebug() << "Manual circuit connection completed";
    
    // Test the circuit
    arduino->powerOn();
    arduino->pinMode(13, Arduino::OUTPUT);
    arduino->digitalWrite(13, Arduino::HIGH);
    
    ArduinoPin* pin13 = arduino->getPin(13);
    qDebug() << "Pin 13 state:" << (pin13->getVoltage() > 2.5 ? "HIGH" : "LOW");
    
    delete circuit; // Deletes all components except Arduino
    delete arduino; // Need to delete Arduino separately
}


// Temporary main for initial testing
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    testArduinoLEDCircuit();
    testManualArduinoLEDConnection();
    
    QMainWindow window;
    window.setWindowTitle("Arduino Simulator - Initial Test");
    window.resize(800, 600);
    
    QWidget *centralWidget = new QWidget(&window);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    
    QLabel *label = new QLabel("Arduino Simulator - Ready for Development!", centralWidget);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("font-size: 18px; padding: 20px;");
    
    layout->addWidget(label);
    window.setCentralWidget(centralWidget);
    
    window.show();
    
    return app.exec();
}
