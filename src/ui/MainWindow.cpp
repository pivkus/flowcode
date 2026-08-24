#include "MainWindow.hpp"
#include <QVBoxLayout>
#include <QPushButton>

#include "../core/Uuid.hpp" // TODO: this is temporary


MainWindow::MainWindow(Bridge& bridge, QWidget *parent)
 : QWidget(parent), manager(bridge, this), layout(new QHBoxLayout)
{
    setWindowTitle("Main Window");
    resize(800, 600);

    QVBoxLayout *outer = new QVBoxLayout(this);

    QPushButton *new_session_button = new QPushButton("New session", this);
    connect(new_session_button, &QPushButton::clicked, this, &MainWindow::newSession);

    outer->addWidget(new_session_button);
    outer->addLayout(layout, 1);

    setLayout(outer);

    connect(this, &MainWindow::sessionCreated, &bridge, &Bridge::sessionCreated);
}

void MainWindow::newSession()
{
    // TODO: session_ids should be generated on the backend
    Uuid session_id = Uuid::generate_v7();

    SessionWidget* w = manager.createSession(session_id);
    layout->addWidget(w, 1);
    emit sessionCreated(session_id);
}
