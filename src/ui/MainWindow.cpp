#include "MainWindow.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

#include "../core/Uuid.hpp" // TODO: this is temporary


MainWindow::MainWindow(Bridge& bridge, QWidget *parent)
 : QWidget(parent), manager(bridge, this), layout(new QHBoxLayout)
{
    setWindowTitle("Main Window");
    resize(800, 600);
    
    QHBoxLayout *root = new QHBoxLayout(this);
    sidebar = new Sidebar(bridge, this);

    QVBoxLayout *main_column = new QVBoxLayout;

    QPushButton *new_session_button = new QPushButton("New session", this);
    connect(new_session_button, &QPushButton::clicked, this, &MainWindow::newSession);

    main_column->addWidget(new_session_button);
    main_column->addLayout(layout, 1);

    root->addWidget(sidebar);
    root->addLayout(main_column, 1);

    setLayout(root);

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
