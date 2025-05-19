#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>

class CircuitCanvas;
class ComponentLibrary;
class Circuit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void newCircuit();
    void openCircuit();
    void saveCircuit();
    void exitApplication();

    void startSimulation();
    void stopSimulation();

    void about();

private:
    void setupUi();
    void createMenus();
    void createToolBars();
    void createStatusBar();

    CircuitCanvas *m_circuitCanvas;
    ComponentLibrary *m_componentLibrary;
    Circuit *m_circuit;

    // Menus
    QMenu *m_fileMenu;
    QMenu *m_simulationMenu;
    QMenu *m_helpMenu;

    // Toolbars
    QToolBar *m_fileToolBar;
    QToolBar *m_simulationToolBar;

    // Actions
    QAction *m_newAction;
    QAction *m_openAction;
    QAction *m_saveAction;
    QAction *m_exitAction;
    QAction *m_startSimAction;
    QAction *m_stopSimAction;
    QAction *m_aboutAction;
};

#endif // MAINWINDOW_H
