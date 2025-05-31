#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QWidget>
#include <QGraphicsView>
#include <QDebug>
#include <QTimer>
#include <QGroupBox>

// Include UI components
#include "ui/CircuitCanvas.h"
#include "ui/LEDGraphicsItem.h"
#include "ui/WireGraphicsItem.h"
#include "ui/ArduinoGraphicsItem.h"

// Include backend components
#include "simulation/Circuit.h"
#include "simulation/CircuitSimulator.h"
#include "core/LED.h"
#include "core/Arduino.h"

class LEDWireTestWindow : public QMainWindow
{
    Q_OBJECT

public:
    LEDWireTestWindow(QWidget* parent = nullptr)
        : QMainWindow(parent)
        , m_circuit(nullptr)
        , m_simulator(nullptr)
        , m_arduino(nullptr)
        , m_circuitCanvas(nullptr)
        , m_graphicsView(nullptr)
    {
        setupUI();
        setupCircuit();
    }

    ~LEDWireTestWindow()
    {
        cleanup();
    }

private slots:
    void addLED()
    {
        if (!m_circuitCanvas) return;
        
        static int ledCount = 0;
        QPointF position(100 + (ledCount * 150), 200);
        
        QColor colors[] = {Qt::red, Qt::green, Qt::blue, Qt::yellow};
        QColor color = colors[ledCount % 4];
        
        ComponentGraphicsItem* led = m_circuitCanvas->addLED(position, color);
        if (led) {
            m_statusLabel->setText(QString("Added LED %1 at (%2, %3)")
                                  .arg(ledCount + 1)
                                  .arg(position.x())
                                  .arg(position.y()));
            ledCount++;
        }
    }

    void addArduino()
    {
        if (!m_circuitCanvas) return;
        
        QPointF position(50, 50);
        ComponentGraphicsItem* arduino = m_circuitCanvas->addArduino(position);
        if (arduino) {
            m_statusLabel->setText("Added Arduino at (50, 50)");
        } else {
            m_statusLabel->setText("Arduino graphics not yet implemented");
        }
    }

    void clearCanvas()
    {
        if (!m_circuitCanvas) return;
        
        // Clear all components
        const auto& components = m_circuitCanvas->getComponents();
        for (int i = components.size() - 1; i >= 0; --i) {
            m_circuitCanvas->removeComponent(components[i]);
        }
        
        // Clear all wires
        const auto& wires = m_circuitCanvas->getWires();
        for (int i = wires.size() - 1; i >= 0; --i) {
            m_circuitCanvas->removeItem(wires[i]);
            delete wires[i];
        }
        
        m_statusLabel->setText("Canvas cleared");
    }

    void startSimulation()
    {
        if (!m_simulator) return;
        
        m_simulator->start();
        m_simulationButton->setText("Stop Simulation");
        m_simulationButton->disconnect();
        connect(m_simulationButton, &QPushButton::clicked, this, &LEDWireTestWindow::stopSimulation);
        
        m_statusLabel->setText("Simulation started");
    }

    void stopSimulation()
    {
        if (!m_simulator) return;
        
        m_simulator->stop();
        m_simulationButton->setText("Start Simulation");
        m_simulationButton->disconnect();
        connect(m_simulationButton, &QPushButton::clicked, this, &LEDWireTestWindow::startSimulation);
        
        m_statusLabel->setText("Simulation stopped");
    }

    void powerArduino()
    {
        if (!m_arduino) return;
        
        if (m_arduino->isPoweredOn()) {
            m_arduino->powerOff();
            m_powerButton->setText("Power On Arduino");
            m_statusLabel->setText("Arduino powered off");
        } else {
            m_arduino->powerOn();
            m_powerButton->setText("Power Off Arduino");
            m_statusLabel->setText("Arduino powered on");
        }
    }

    void configurePin13()
    {
        if (!m_arduino || !m_arduino->isPoweredOn()) {
            m_statusLabel->setText("Arduino not powered on");
            return;
        }
        
        m_arduino->pinMode(13, Arduino::OUTPUT);
        m_statusLabel->setText("Pin 13 configured as OUTPUT");
    }

    void togglePin13()
    {
        if (!m_arduino || !m_arduino->isPoweredOn()) {
            m_statusLabel->setText("Arduino not powered on");
            return;
        }
        
        static bool pin13State = false;
        pin13State = !pin13State;
        
        m_arduino->digitalWrite(13, pin13State ? Arduino::HIGH : Arduino::LOW);
        m_pin13Button->setText(pin13State ? "Pin 13: HIGH" : "Pin 13: LOW");
        
        m_statusLabel->setText(QString("Pin 13 set to %1").arg(pin13State ? "HIGH" : "LOW"));
    }

    void onWireDrawingStarted(ComponentGraphicsItem* component, int terminal)
    {
        m_statusLabel->setText(QString("Wire drawing started from %1 terminal %2")
                              .arg(component->getComponentName())
                              .arg(terminal));
    }

    void onWireDrawingCompleted(bool success)
    {
        if (success) {
            m_statusLabel->setText("Wire connection created successfully");
        } else {
            m_statusLabel->setText("Wire connection failed");
        }
    }

    void onWireCreated(WireGraphicsItem* wire)
    {
        m_statusLabel->setText("New wire created in circuit");
        
        // Show debug info about the wire
        qDebug() << "Wire created between"
                 << wire->getStartComponent()->getComponentName()
                 << "and" << wire->getEndComponent()->getComponentName();
    }

private:
    void setupUI()
    {
        setWindowTitle("LED and Wire Drawing Test");
        resize(1200, 800);
        
        // Create central widget
        QWidget* centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);
        
        // Main layout
        QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
        
        // Left panel for controls
        QWidget* controlPanel = new QWidget();
        controlPanel->setFixedWidth(250);
        controlPanel->setStyleSheet("background-color: #f0f0f0; border-right: 1px solid #ccc;");
        
        QVBoxLayout* controlLayout = new QVBoxLayout(controlPanel);
        
        // Component controls
        QGroupBox* componentGroup = new QGroupBox("Components");
        QVBoxLayout* componentLayout = new QVBoxLayout(componentGroup);
        
        QPushButton* addLEDBtn = new QPushButton("Add LED");
        QPushButton* addArduinoBtn = new QPushButton("Add Arduino");
        QPushButton* clearBtn = new QPushButton("Clear Canvas");
        
        componentLayout->addWidget(addLEDBtn);
        componentLayout->addWidget(addArduinoBtn);
        componentLayout->addWidget(clearBtn);
        
        connect(addLEDBtn, &QPushButton::clicked, this, &LEDWireTestWindow::addLED);
        connect(addArduinoBtn, &QPushButton::clicked, this, &LEDWireTestWindow::addArduino);
        connect(clearBtn, &QPushButton::clicked, this, &LEDWireTestWindow::clearCanvas);
        
        // Simulation controls
        QGroupBox* simGroup = new QGroupBox("Simulation");
        QVBoxLayout* simLayout = new QVBoxLayout(simGroup);
        
        m_simulationButton = new QPushButton("Start Simulation");
        connect(m_simulationButton, &QPushButton::clicked, this, &LEDWireTestWindow::startSimulation);
        simLayout->addWidget(m_simulationButton);
        
        // Arduino controls
        QGroupBox* arduinoGroup = new QGroupBox("Arduino Control");
        QVBoxLayout* arduinoLayout = new QVBoxLayout(arduinoGroup);
        
        m_powerButton = new QPushButton("Power On Arduino");
        QPushButton* configPin13Btn = new QPushButton("Configure Pin 13");
        m_pin13Button = new QPushButton("Pin 13: LOW");
        
        connect(m_powerButton, &QPushButton::clicked, this, &LEDWireTestWindow::powerArduino);
        connect(configPin13Btn, &QPushButton::clicked, this, &LEDWireTestWindow::configurePin13);
        connect(m_pin13Button, &QPushButton::clicked, this, &LEDWireTestWindow::togglePin13);
        
        arduinoLayout->addWidget(m_powerButton);
        arduinoLayout->addWidget(configPin13Btn);
        arduinoLayout->addWidget(m_pin13Button);
        
        // Instructions
        QGroupBox* instructionGroup = new QGroupBox("Instructions");
        QVBoxLayout* instructionLayout = new QVBoxLayout(instructionGroup);
        
        QLabel* instructions = new QLabel(
            "1. Add LEDs and Arduino using the buttons\n"
            "2. Click on connection points to start wire drawing\n"
            "3. Click on another connection point to complete wire\n"
            "4. Right-click or ESC to cancel wire drawing\n"
            "5. Start simulation to see electrical behavior\n"
            "6. Use Arduino controls to test LED circuits"
        );
        instructions->setWordWrap(true);
        instructions->setStyleSheet("font-size: 10px; color: #666;");
        instructionLayout->addWidget(instructions);
        
        // Add groups to control layout
        controlLayout->addWidget(componentGroup);
        controlLayout->addWidget(simGroup);
        controlLayout->addWidget(arduinoGroup);
        controlLayout->addWidget(instructionGroup);
        controlLayout->addStretch();
        
        // Status label
        m_statusLabel = new QLabel("Ready - Add some LEDs and try connecting them with wires");
        m_statusLabel->setStyleSheet("font-weight: bold; padding: 5px; background-color: #e0e0e0; border: 1px solid #ccc;");
        controlLayout->addWidget(m_statusLabel);
        
        // Right panel for circuit canvas
        m_circuitCanvas = new CircuitCanvas();
        m_graphicsView = new QGraphicsView(m_circuitCanvas);
        m_graphicsView->setDragMode(QGraphicsView::RubberBandDrag);
        m_graphicsView->setRenderHint(QPainter::Antialiasing);
        
        // Connect canvas signals
        connect(m_circuitCanvas, &CircuitCanvas::wireDrawingStarted,
                this, &LEDWireTestWindow::onWireDrawingStarted);
        connect(m_circuitCanvas, &CircuitCanvas::wireDrawingCompleted,
                this, &LEDWireTestWindow::onWireDrawingCompleted);
        connect(m_circuitCanvas, &CircuitCanvas::wireCreated,
                this, &LEDWireTestWindow::onWireCreated);
        
        // Add to main layout
        mainLayout->addWidget(controlPanel);
        mainLayout->addWidget(m_graphicsView, 1); // Give graphics view more space
    }

    void setupCircuit()
    {
        // Create circuit and simulator
        m_circuit = new Circuit(this);
        m_simulator = new CircuitSimulator(m_circuit, this);
        
        // Set circuit for canvas
        m_circuitCanvas->setCircuit(m_circuit);
        
        // Create Arduino for testing
        m_arduino = new Arduino(Arduino::UNO, this);
        m_arduino->setCircuit(m_circuit);
        
        // Connect simulator signals
        connect(m_simulator, &CircuitSimulator::simulationStarted, [this]() {
            m_statusLabel->setText("✓ Simulation running");
        });
        
        connect(m_simulator, &CircuitSimulator::simulationStopped, [this]() {
            m_statusLabel->setText("Simulation stopped");
        });
        
        connect(m_simulator, &CircuitSimulator::convergenceAchieved, [this]() {
            // Update UI after convergence
            QTimer::singleShot(10, [this]() {
                m_statusLabel->setText("✓ Simulation converged");
            });
        });
        
        connect(m_simulator, &CircuitSimulator::convergenceFailed, [this](int iterations) {
            m_statusLabel->setText(QString("✗ Convergence failed after %1 iterations").arg(iterations));
        });
        
        connect(m_simulator, &CircuitSimulator::simulationError, [this](const QString& error) {
            m_statusLabel->setText("✗ Simulation error: " + error);
        });
        
        qDebug() << "Circuit and simulator setup complete";
    }

    void cleanup()
    {
        if (m_simulator) {
            m_simulator->stop();
        }
        
        if (m_arduino) {
            m_arduino->powerOff();
        }
    }

private:
    // Backend objects
    Circuit* m_circuit;
    CircuitSimulator* m_simulator;
    Arduino* m_arduino;
    
    // UI objects
    CircuitCanvas* m_circuitCanvas;
    QGraphicsView* m_graphicsView;
    QLabel* m_statusLabel;
    QPushButton* m_simulationButton;
    QPushButton* m_powerButton;
    QPushButton* m_pin13Button;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    qDebug() << "=== LED and Wire Drawing Test Application ===";
    qDebug() << "Testing LEDGraphicsItem and WireGraphicsItem functionality";
    
    LEDWireTestWindow window;
    window.show();
    
    qDebug() << "Application ready for testing";
    qDebug() << "- Add LEDs using the button";
    qDebug() << "- Click on LED connection points to start wire drawing";
    qDebug() << "- Click on another connection point to complete the wire";
    
    return app.exec();
}

#include "test_led_wire_main.moc"