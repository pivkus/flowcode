#include "MainWindow.hpp"
#include <QVBoxLayout>


MainWindow::MainWindow(Bridge& bridge, QWidget *parent) 
 : QWidget(parent), manager(bridge, this), layout(new QHBoxLayout(this))
{
    setWindowTitle("Main Window");
    resize(800, 600);

    SessionWidget* w1 = manager.createSession(1);
    SessionWidget* w2 = manager.createSession(2);


    layout->addWidget(w1, 1);
    layout->addWidget(w2, 1);

    setLayout(layout);
}
