#include "ui/MainWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_circuitCanvas(nullptr)
    , m_componentLibrary(nullptr)
    , m_circuit(nullptr)
{
    setupUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle("Arduino Simulator");
    resize(1200, 800);
    
    // TODO: Implement UI setup
}

void MainWindow::newCircuit() { /* TODO */ }
void MainWindow::openCircuit() { /* TODO */ }
void MainWindow::saveCircuit() { /* TODO */ }
void MainWindow::exitApplication() { close(); }
void MainWindow::startSimulation() { /* TODO */ }
void MainWindow::stopSimulation() { /* TODO */ }
void MainWindow::about() { /* TODO */ }
void MainWindow::createMenus() { /* TODO */ }
void MainWindow::createToolBars() { /* TODO */ }
void MainWindow::createStatusBar() { /* TODO */ }
