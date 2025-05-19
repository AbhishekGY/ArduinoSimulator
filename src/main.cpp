#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QLabel>
#include <QWidget>

// Temporary main for initial testing
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
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
