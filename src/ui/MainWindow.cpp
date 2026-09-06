#include "MainWindow.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

#include "../core/Uuid.hpp" // TODO: this is temporary


MainWindow::MainWindow(Bridge& bridge, QWidget *parent)
 : QWidget(parent), bridge(bridge) {
    setWindowTitle("Main Window");
    resize(800, 600);
    
    connect(&bridge, &Bridge::sessionLoadReceived, this, &MainWindow::onSessionLoaded);
    
    QHBoxLayout *root = new QHBoxLayout(this);
    
    sidebar = new Sidebar(bridge, this);
    connect(sidebar, &Sidebar::sessionSelected, this, &MainWindow::focusSession);
    connect(sidebar, &Sidebar::newSession, this, &MainWindow::newSession);
    
    stack = new SessionStack(bridge, this);

    root->addWidget(sidebar);
    root->addWidget(stack, 1);

    setLayout(root);

    connect(this, &MainWindow::sessionCreated, &bridge, &Bridge::sessionCreated);
}

void MainWindow::newSession()
{
    // TODO: session_ids should be generated on the backend
    Uuid id = Uuid::generate_v7();

    SessionWidget* w = stack->create(id);
    sidebar->addSelectSession(id);
    pending_focus.reset();
    emit sessionCreated(id);
}

void MainWindow::focusSession(Uuid id){
    if (SessionWidget* w = stack->get(id)){
        stack->setCurrentWidget(w);
        pending_focus.reset();
        return;
    }

    pending_focus = id;
    bridge.sessionLoadRequested(id);
}

void MainWindow::onSessionLoaded(Uuid id, TurnVec history){
    if (stack->get(id)) return;

    SessionWidget* w = stack->create(id);
    w->renderSession(history);

    if (pending_focus == id){
        stack->setCurrentWidget(w);
        pending_focus.reset();
    }
}